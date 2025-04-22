import os
import time
import subprocess
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

class AudioHandler(FileSystemEventHandler):
    def __init__(self):
        self.playing = False
        self.file_queue = []
        self.current_process = None

    def on_created(self, event):
        """处理所有新文件创建事件"""
        if not event.is_directory:
            self.file_queue.append(event.src_path)
            self.play_next()

    def is_valid_mp3(self, filepath):
        """通过文件头验证MP3文件"""
        try:
            with open(filepath, 'rb') as f:
                header = f.read(3)
                # 检查ID3标签或MPEG帧头
                return header == b'ID3' or (len(header) == 3 and 
                       header[0] == 0xFF and (header[1] & 0xE0) == 0xE0)
        except Exception as e:
            print(f"文件校验失败: {str(e)}")
            return False

    def wait_for_upload_completion(self, filepath):
        """智能文件等待机制"""
        print(f"开始监控: {os.path.basename(filepath)}")
        max_wait = 15  # 总等待时间15秒
        check_interval = 0.3
        stable_threshold = 2  # 2秒无变化视为稳定
        
        last_size = -1
        stable_start = None
        start_time = time.time()
        
        while time.time() - start_time < max_wait:
            try:
                # 存在性检查
                if not os.path.exists(filepath):
                    print("文件暂未出现...")
                    time.sleep(check_interval)
                    continue
                    
                current_size = os.path.getsize(filepath)
                
                # 有效性检查
                if current_size == 0:
                    print("等待文件初始化...")
                    time.sleep(check_interval)
                    continue
                    
                # 稳定性检测
                if current_size == last_size:
                    if stable_start is None:
                        stable_start = time.time()
                    elif time.time() - stable_start >= stable_threshold:
                        print("文件已稳定")
                        return True
                else:
                    stable_start = None
                    last_size = current_size
                    
                print(f"大小变化: {last_size} → {current_size}")
                time.sleep(check_interval)
                
            except Exception as e:
                print(f"监控异常: {str(e)}")
                time.sleep(check_interval)
        
        print("等待超时")
        return False

    def play_next(self):
        if self.playing or not self.file_queue:
            return

        current_file = self.file_queue.pop(0)
        
        try:
            # 阶段1：等待文件就绪
            if not self.wait_for_upload_completion(current_file):
                os.remove(current_file)
                return

            # 阶段2：验证文件类型
            if not self.is_valid_mp3(current_file):
                print(f"非MP3文件已删除: {os.path.basename(current_file)}")
                os.remove(current_file)
                return

            # 阶段3：播放流程
            self.playing = True
            print(f"开始播放: {os.path.basename(current_file)}")
            
            ffmpeg_cmd = [
                'ffmpeg',
                '-hide_banner',
                '-loglevel', 'error',
                '-i', current_file,
                '-af', 'volume=+10dB',
                '-ac', '1',
                '-ar', '44100',
                '-acodec', 'pcm_s16le',
                '-f', 's16le', '-'
            ]
            
            aplay_cmd = [
                'aplay',
                '-D', 'hw:1,0',
                '-f', 'S16_LE',
                '-c', '1',
                '-r', '44100',
                '--buffer-time=1000'
            ]

            ffmpeg_proc = subprocess.Popen(
                ffmpeg_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )

            self.current_process = subprocess.Popen(
                aplay_cmd,
                stdin=ffmpeg_proc.stdout,
                stderr=subprocess.PIPE
            )

            # 等待播放完成
            ffmpeg_proc.wait()
            if self.current_process.poll() is None:
                self.current_process.wait()

        except Exception as e:
            print(f"播放异常: {str(e)}")
        finally:
            # 清理资源
            self.cleanup_resources(current_file)

    def cleanup_resources(self, filepath):
        """资源清理"""
        # 终止残留进程
        if self.current_process and self.current_process.poll() is None:
            self.current_process.terminate()
        
        # 删除文件
        try:
            if os.path.exists(filepath):
                os.remove(filepath)
                print(f"已清理文件: {os.path.basename(filepath)}")
        except Exception as e:
            print(f"文件清理失败: {str(e)}")
        
        # 状态重置
        self.playing = False
        self.current_process = None
        self.play_next()  # 继续播放队列

if __name__ == "__main__":
    monitor_path = "/var/www/mp3"
    event_handler = AudioHandler()
    observer = Observer()
    observer.schedule(event_handler, monitor_path, recursive=False)
    observer.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        observer.stop()
    observer.join()