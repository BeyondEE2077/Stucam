import subprocess
import threading
from datetime import datetime
from zoneinfo import ZoneInfo
import os
import time

# 全局状态控制
is_recording = False
rec_process = None
v_recorder = "/mnt/system/usr/bin/recorder_h265"  # 视频录制主程序
audio_process = None  # 新增音频录制进程
video_path = None      # 视频文件路径
audio_path = None     # 音频文件路径
led_path = "/sys/class/leds/led-user/trigger"
current_led_mode = "activity"

# 音频偏移常量（单位：秒）
AUDIO_OFFSET = "-0.8"  # 根据实际情况调整，正值：音频延后；负值：音频提前
VOLUME_LEVEL = "18"   # 录音音量
VIDEO_FPS = "30"  # 帧率，用于FFMPEG参数
AUDIO_SAMPLE_RATE = "8000" #音频采样率

# 预加载音频数据
hello_wav_data = None
ready_wav_data = None
finish_wav_data = None

def load_audio_files():
    """预加载音频文件到内存（包含hello.wav）"""
    global hello_wav_data, ready_wav_data, finish_wav_data  # 添加hello
    try:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        
        # 加载hello.wav
        hello_path = os.path.join(script_dir, "hello.wav")  # 新增路径
        with open(hello_path, "rb") as f:
            hello_wav_data = f.read()
            
        # 原有加载逻辑保持不变
        ready_path = os.path.join(script_dir, "ready.wav")
        finish_path = os.path.join(script_dir, "finish.wav")
        with open(ready_path, "rb") as f:
            ready_wav_data = f.read()
        with open(finish_path, "rb") as f:
            finish_wav_data = f.read()
    except FileNotFoundError as e:
        print(f"音频文件未找到: {e.filename}")
    except Exception as e:
        print(f"音频文件加载失败: {str(e)}")

def play_audio(wav_data):
    """从内存数据播放音频（同步阻塞方式）"""
    if not wav_data:
        return
    try:
        proc = subprocess.Popen(
            ["aplay", "-D", "hw:1,0", "-f", "S16_LE", "-"],
            stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        proc.stdin.write(wav_data)
        proc.stdin.close()
        proc.wait()  # 阻塞直到播放完成
    except Exception as e:
        print(f"音频播放失败: {str(e)}")

def get_local_timestamp():
    local_time = datetime.now(ZoneInfo('Asia/Shanghai'))
    return local_time.strftime("%Y%m%d_%H_%M_%S")

def set_led_mode(mode):
    global current_led_mode
    try:
        with open(led_path, 'w') as f:
            f.write(f"{mode}\n")
            current_led_mode = mode
    except Exception as e:
        print(f"LED控制失败: {str(e)}")
        os.system(f"echo {mode} > {led_path} 2>/dev/null")

def start_recording():
    global rec_process, audio_process, is_recording, video_path, audio_path
    if is_recording:
        return

    # 同步播放开始提示音
    play_audio(ready_wav_data)

    timestamp = get_local_timestamp()
    video_path = f"/var/www/tmp/vtmp_{timestamp}.vraw"
    audio_path = f"/var/www/tmp/atmp_{timestamp}.wav"

    try:
        # 设置录音音量
        subprocess.run(
            ["amixer", "-Dhw:0", "cset", "name='ADC Capture Volume'", VOLUME_LEVEL],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )

        # 确保目录存在
        os.makedirs(os.path.dirname(video_path), exist_ok=True)
        os.makedirs(os.path.dirname(audio_path), exist_ok=True)

        # 启动视频录制
        rec_process = subprocess.Popen(
            [v_recorder, video_path, "7200"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )

        # 启动音频录制
        audio_process = subprocess.Popen(
            ["arecord", "-Dhw:0,0", "-r", AUDIO_SAMPLE_RATE, "-f", "S16_LE", "-t", "wav", audio_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )

        is_recording = True
        set_led_mode("heartbeat")
        print(f"录制已启动 视频: {video_path}, 音频: {audio_path}")
    except subprocess.CalledProcessError as e:
        print(f"音量设置失败: {str(e)}")
        if rec_process and rec_process.poll() is None:
            rec_process.terminate()
            rec_process.wait()
        set_led_mode("activity")
        is_recording = False
    except Exception as e:
        print(f"录制启动失败: {str(e)}")
        if rec_process and rec_process.poll() is None:
            rec_process.terminate()
            rec_process.wait()
        if audio_process and audio_process.poll() is None:
            audio_process.terminate()
            audio_process.wait()
        set_led_mode("activity")
        is_recording = False

def stop_recording():
    global rec_process, audio_process, is_recording, video_path, audio_path
    if not is_recording:
        return

    is_recording = False
    video_file = video_path
    audio_file = audio_path

    try:
        # 终止视频录制
        if rec_process and rec_process.poll() is None:
            rec_process.terminate()
            try:
                rec_process.wait(5)
            except subprocess.TimeoutExpired:
                rec_process.kill()
                rec_process.wait()
        
        # 终止音频录制
        if audio_process and audio_process.poll() is None:
            audio_process.terminate()
            try:
                audio_process.wait(5)
            except subprocess.TimeoutExpired:
                audio_process.kill()
                audio_process.wait()
    except Exception as e:
        print(f"终止录制异常: {str(e)}")
    finally:
        set_led_mode("activity")
        rec_process = None
        audio_process = None

    # 合并音视频
    success = False
    if video_file and audio_file and os.path.exists(video_file) and os.path.exists(audio_file):
        success = convert_to_mp4(video_file, audio_file)
    else:
        print("警告: 视频或音频文件缺失")

    # 播放完成音效
    if success:
        play_audio(finish_wav_data)

def convert_to_mp4(video_path, audio_path):
    mp4_filename = f"v_{get_local_timestamp()}.mp4"
    mp4_path = f"/var/www/v/{mp4_filename}"
    
    try:
        os.makedirs(os.path.dirname(mp4_path), exist_ok=True)
        
        # 构建ffmpeg命令（应用音频偏移）
        cmd = [
            "ffmpeg",
            "-y",
            "-framerate", VIDEO_FPS,
            "-i", video_path,
            "-itsoffset", AUDIO_OFFSET,
            "-i", audio_path,
            "-c:v", "copy",
            "-c:a", "aac",
            "-shortest",
            mp4_path
        ]
        
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        if result.returncode == 0:
            print(f"文件合并成功: {mp4_path}")
            os.remove(video_path)
            os.remove(audio_path)
            return True
        else:
            print(f"合并失败:\n{result.stderr}")
            return False
    except Exception as e:
        print(f"合并异常: {str(e)}")
        return False
    finally:
        # 清理残留文件（可选）
        if 'video_path' in locals() and os.path.exists(video_path):
            print(f"残留视频文件: {video_path}")
        if 'audio_path' in locals() and os.path.exists(audio_path):
            print(f"残留音频文件: {audio_path}")

def monitor_button_events():
    target_components = [
        "type 1 (EV_KEY)",
        "code 431 (KEY_DISPLAYTOGGLE)",
        "value 0"
    ]
    
    proc = subprocess.Popen(
        ["evtest", "/dev/input/event0"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        bufsize=0
    )
    
    buffer = b''
    while True:
        chunk = os.read(proc.stdout.fileno(), 512)
        if not chunk:
            break
        
        buffer += chunk
        while b'\n' in buffer:
            line_bytes, _, buffer = buffer.partition(b'\n')
            line = line_bytes.decode('ascii', errors='ignore').strip()
            
            if line.startswith("Event:") and all(c in line for c in target_components):
                handle_button_action()

def handle_button_action():
    global is_recording
    try:
        if is_recording:
            stop_recording()
        else:
            start_recording()
    except Exception as e:
        print(f"按钮处理异常: {str(e)}")
        set_led_mode("activity")
        is_recording = False

if __name__ == "__main__":
    # 预加载音频文件（包含hello.wav）
    load_audio_files()
    
    # 初始化LED状态
    set_led_mode("activity")
    
    # 播放启动提示音
    play_audio(hello_wav_data)  # 新增播放语句
    
    # 启动监听线程
    button_thread = threading.Thread(target=monitor_button_events, daemon=True)
    button_thread.start()
    
    # 原有主循环保持不变
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n系统关闭中...")
        if is_recording:
            stop_recording()