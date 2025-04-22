#include "cvi_venc.h"
#include <stdbool.h>
#include <stdio.h>
#include <cvi_sys.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sample_comm.h>

#define FRAMERATE (30)

static SAMPLE_VI_CONFIG_S g_stViConfig;
static SAMPLE_INI_CFG_S g_stIniCfg;
static volatile int g_stop_flag = 0;  // 修改为普通volatile int类型

typedef struct {
    const char *filename;
    int seconds;
    pthread_t main_thread_id;  // 新增主线程ID字段
} ThreadParams;

// 函数声明
static void sys_cleanup(void);
static int sys_vi_init(void);
static void sys_vi_deinit(void);
static void sys_venc_deinit(void);
static int sys_venc_init(void);
static int venc_save_stream_one(FILE *fp);


static void *recording_thread(void *arg);

// 添加线程同步机制
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;


// 改进资源释放顺序
static void sys_cleanup(void)
{
    static bool cleaned = false;
    pthread_mutex_lock(&g_lock);
    if (!cleaned) {
        printf("[%s] Starting ordered resource cleanup\n", __func__);
        
        // 1. 先停止编码器接收
        if (CVI_VENC_StopRecvFrame(0) != CVI_SUCCESS) {
            printf("Warning: VENC StopRecvFrame failed\n");
        }
        
        // 2. 解除VPSS-VENC绑定（关键步骤）
        SAMPLE_COMM_VPSS_UnBind_VENC(0, 0, 0);
        
        // 3. 销毁编码器（必须在前两步之后）
        sys_venc_deinit();
        
        // 4. 停止VPSS（等待SDK内部线程处理）
        CVI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {1};
        SAMPLE_COMM_VPSS_Stop(0, abChnEnable);
        usleep(100000); // 给SDK线程时间完成操作
        
        // 5. 最后释放VI资源
        sys_vi_deinit();
        
        printf("[%s] Cleanup completed\n", __func__);
        cleaned = true;
    }
    pthread_mutex_unlock(&g_lock);
}

// recording_thread实现
static void *recording_thread(void *arg)
{
    ThreadParams *params = (ThreadParams *)arg;
    const char *filename = params->filename;
    int framecnt = params->seconds * FRAMERATE;
    FILE *fp = NULL;
    CVI_S32 ret = CVI_SUCCESS;

    // 阶段1: 初始化VI
    printf("[Init] Starting VI initialization\n");
    if (sys_vi_init() != CVI_SUCCESS) {
        fprintf(stderr, "VI init failed\n");
        ret = CVI_FAILURE;
        goto exit;
    }

    // 阶段2: 初始化VENC
    printf("[Init] Starting VENC initialization\n");
    if (sys_venc_init() != CVI_SUCCESS) {
        fprintf(stderr, "VENC init failed\n");
        ret = CVI_FAILURE;
        goto vi_cleanup;
    }

    // 阶段3: 打开输出文件
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", filename);
        ret = CVI_FAILURE;
        goto venc_cleanup;
    }

    // 阶段4: 主采集循环
    printf("[Main] Start recording (frames: %d)\n", framecnt);
    while (framecnt-- > 0) {
        // 优先检查停止标志
        pthread_mutex_lock(&g_lock);
        if (g_stop_flag) {
            pthread_mutex_unlock(&g_lock);
            printf("[Main] Early termination requested\n");
            break;
        }
        pthread_mutex_unlock(&g_lock);

        // 获取并保存视频流
        ret = venc_save_stream_one(fp);
        if (ret != CVI_SUCCESS) {
            printf("[Main] Stream saving error: 0x%x\n", ret);
            break;
        }
    }

    // 阶段5: 有序资源释放
	// 移除录像线程中主动释放资源的代码，确保资源仅由主线程的sys_cleanup函数统一释放，避免重复释放导致的错误。
venc_cleanup:
    printf("[%s Cleanup to do] Releasing VENC resources\n", __func__);
    //sys_venc_deinit();

vi_cleanup:
    printf("[%s Cleanup to do] Releasing VI resources\n", __func__);
    //sys_vi_deinit();

exit:
    if (fp) fclose(fp);
    printf("[Exit] Thread exiting with code: 0x%x\n", ret);
	
    // 使用pthread_kill向主线程发送信号
    pthread_kill(params->main_thread_id, SIGUSR1);

    return (void *)(intptr_t)ret;
}

// 参数检查函数
static int check_arguments(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <output_file> <time_seconds>\n", argv[0]);
        return -1;
    }

    if (strlen(argv[1]) == 0) {
        fprintf(stderr, "Error: Output filename cannot be empty\n");
        return -1;
    }

    char *endptr;
    long seconds = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || seconds <= 0) {
        fprintf(stderr, "Error: Invalid time value. Please enter positive integer\n");
        return -1;
    }

    return 0;
}


CVI_S32 sys_vpss_init(VPSS_GRP VpssGrp, SIZE_S stSizeIn, SIZE_S stSizeOut)
{
	VPSS_GRP_ATTR_S stVpssGrpAttr;
	VPSS_CHN VpssChn = VPSS_CHN0;
	CVI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = { 0 };
	VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
	CVI_S32 s32Ret = CVI_SUCCESS;

	stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
	stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
	stVpssGrpAttr.enPixelFormat =
		SAMPLE_PIXEL_FORMAT; // PIXEL_FORMAT_YUYV / SAMPLE_PIXEL_FORMAT / PIXEL_FORMAT_UYVY / PIXEL_FORMAT_YUV_400
	stVpssGrpAttr.u32MaxW = stSizeIn.u32Width;
	stVpssGrpAttr.u32MaxH = stSizeIn.u32Height;
	stVpssGrpAttr.u8VpssDev = 0;

	astVpssChnAttr[VpssChn].u32Width = stSizeOut.u32Width;
	astVpssChnAttr[VpssChn].u32Height = stSizeOut.u32Height;
	astVpssChnAttr[VpssChn].enVideoFormat = VIDEO_FORMAT_LINEAR;
	astVpssChnAttr[VpssChn].enPixelFormat = SAMPLE_PIXEL_FORMAT;
	astVpssChnAttr[VpssChn].stFrameRate.s32SrcFrameRate = FRAMERATE;
	astVpssChnAttr[VpssChn].stFrameRate.s32DstFrameRate = FRAMERATE;
	astVpssChnAttr[VpssChn].u32Depth = 0;
	astVpssChnAttr[VpssChn].bMirror = CVI_FALSE;
	astVpssChnAttr[VpssChn].bFlip = CVI_FALSE;
	astVpssChnAttr[VpssChn].stAspectRatio.enMode = ASPECT_RATIO_AUTO;
	astVpssChnAttr[VpssChn].stAspectRatio.bEnableBgColor = CVI_TRUE;
	astVpssChnAttr[VpssChn].stAspectRatio.u32BgColor = COLOR_RGB_BLACK;
	astVpssChnAttr[VpssChn].stNormalize.bEnable = CVI_FALSE;

	/*start vpss*/
	abChnEnable[0] = CVI_TRUE;
	s32Ret = SAMPLE_COMM_VPSS_Init(VpssGrp, abChnEnable, &stVpssGrpAttr,
				       astVpssChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		SAMPLE_PRT("init vpss group failed. s32Ret: 0x%x !\n", s32Ret);
		goto error;
	}

	s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr,
					astVpssChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
		goto error;
	}

	return s32Ret;
error:
	return s32Ret;
}

static int sys_vi_init(void)
{
	MMF_VERSION_S stVersion;
	SAMPLE_INI_CFG_S stIniCfg;
	SAMPLE_VI_CONFIG_S stViConfig;

	PIC_SIZE_E enPicSize;
	SIZE_S stSize;
	CVI_S32 s32Ret = CVI_SUCCESS;
	LOG_LEVEL_CONF_S log_conf;

	CVI_SYS_GetVersion(&stVersion);
	SAMPLE_PRT("MMF Version:%s\n", stVersion.version);

	log_conf.enModId = CVI_ID_LOG;
	log_conf.s32Level = CVI_DBG_INFO;
	CVI_LOG_SetLevelConf(&log_conf);

	// Get config from ini if found.
	if (SAMPLE_COMM_VI_ParseIni(&stIniCfg)) {
		SAMPLE_PRT("Parse complete\n");
	}

	//Set sensor number
	CVI_VI_SetDevNum(stIniCfg.devNum);
	/************************************************
	 * step1:  Config VI
	 ************************************************/
	s32Ret = SAMPLE_COMM_VI_IniToViCfg(&stIniCfg, &stViConfig);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	memcpy(&g_stViConfig, &stViConfig, sizeof(SAMPLE_VI_CONFIG_S));
	memcpy(&g_stIniCfg, &stIniCfg, sizeof(SAMPLE_INI_CFG_S));

	printf("Pixel format: %d\n", stViConfig.astViInfo[0].stChnInfo.enPixFormat);
	/************************************************
	 * step2:  Get input size
	 ************************************************/
	s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stIniCfg.enSnsType[0],
						&enPicSize);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_LOG(
			CVI_DBG_ERR,
			"SAMPLE_COMM_VI_GetSizeBySensor failed with %#x\n",
			s32Ret);
		return s32Ret;
	}

	s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
	if (s32Ret != CVI_SUCCESS) {
		SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed with %#x\n",
			   s32Ret);
		return s32Ret;
	}

	/************************************************
	 * step3:  Init modules
	 ************************************************/
	s32Ret = SAMPLE_PLAT_SYS_INIT(stSize);
	if (s32Ret != CVI_SUCCESS) {
		SAMPLE_PRT("sys init failed. s32Ret: 0x%x !\n", s32Ret);
		return s32Ret;
	}

	VI_VPSS_MODE_S vivpssmode;
	VPSS_MODE_S stVPSSMode;
	vivpssmode.aenMode[0] = vivpssmode.aenMode[1] = VI_OFFLINE_VPSS_ONLINE;
	stVPSSMode.enMode = VPSS_MODE_SINGLE;
	stVPSSMode.aenInput[0] = VPSS_INPUT_ISP;
	stVPSSMode.ViPipe[0] = 0;
	s32Ret = CVI_SYS_SetVIVPSSMode(&vivpssmode);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_LOG(CVI_DBG_ERR,
			      "CVI_SYS_SetVIVPSSMode failed with %#x\n",
			      s32Ret);
		return s32Ret;
	}

	s32Ret = CVI_SYS_SetVPSSModeEx(&stVPSSMode);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_LOG(CVI_DBG_ERR,
			      "CVI_SYS_SetVPSSModeEx failed with %#x\n",
			      s32Ret);
		return s32Ret;
	}

	s32Ret = SAMPLE_PLAT_VI_INIT(&stViConfig);
	if (s32Ret != CVI_SUCCESS) {
		SAMPLE_PRT("vi init failed. s32Ret: 0x%x !\n", s32Ret);
		return s32Ret;
	}

	SIZE_S size_out;
	SAMPLE_COMM_SYS_GetPicSize(PIC_720P, &size_out);
	s32Ret = sys_vpss_init(0, stSize, size_out);
	if (s32Ret != CVI_SUCCESS) {
		SAMPLE_PRT("vpss init failed. s32Ret: 0x%x !\n", s32Ret);
		return s32Ret;
	}

	s32Ret = SAMPLE_COMM_VI_Bind_VPSS(0, 0, 0);
	if (s32Ret != CVI_SUCCESS) {
		SAMPLE_PRT("vi bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
		return s32Ret;
	}

	return CVI_SUCCESS;
}

// 修改sys_vi_deinit函数，销毁VPSS组
static void sys_vi_deinit(void)
{
    CVI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = { 1 };
    SAMPLE_COMM_VI_UnBind_VPSS(0, 0, 0);

    SAMPLE_COMM_VPSS_Stop(0, abChnEnable);

    // 添加销毁VPSS组的代码
    CVI_S32 s32Ret = CVI_VPSS_DestroyGrp(0);
    if (s32Ret != CVI_SUCCESS) {
        printf("CVI_VPSS_DestroyGrp failed: 0x%x\n", s32Ret);
    }

    SAMPLE_COMM_VI_DestroyIsp(&g_stViConfig);
    SAMPLE_COMM_VI_DestroyVi(&g_stViConfig);
    SAMPLE_COMM_SYS_Exit();
}

static int sys_venc_init(void)
{
	CVI_S32 s32Ret;
	VENC_CHN ch = 0;
	VENC_CHN_ATTR_S venc_chn_attr = {
		.stVencAttr = {
#if defined(H265_ENABLE)
			.enType = PT_H265,
#else
			.enType = PT_H264,
#endif
			.u32MaxPicWidth = 1280,
			.u32MaxPicHeight = 720,
			.u32BufSize = 1024 * 1024,
			.u32Profile = 66,
			.bByFrame = CVI_TRUE,
			.u32PicWidth = 1280,
			.u32PicHeight = 720,
			.bSingleCore = CVI_TRUE,
			.bEsBufQueueEn = CVI_FALSE,
			.bIsoSendFrmEn = CVI_FALSE,
		},
		.stRcAttr = {
#if defined(H265_ENABLE)
			.enRcMode = VENC_RC_MODE_H265CBR,
			.stH265Cbr.u32Gop = 50,
			.stH265Cbr.u32StatTime = 2,
			.stH265Cbr.u32SrcFrameRate = FRAMERATE,
			.stH265Cbr.fr32DstFrameRate = FRAMERATE,
			.stH265Cbr.u32BitRate = 128,
			.stH265Cbr.bVariFpsEn = CVI_FALSE,

#else
			.enRcMode = VENC_RC_MODE_H264CBR,
			.stH264Cbr = {
				.u32Gop = 60,
				.u32StatTime = 3,
				.u32SrcFrameRate = FRAMERATE,
				.fr32DstFrameRate = FRAMERATE,
				.u32BitRate = 128,
				.bVariFpsEn = CVI_FALSE,
			}
#endif
		},
		.stGopAttr = {
			.enGopMode = VENC_GOPMODE_NORMALP,
			.stNormalP = {
				.s32IPQpDelta = 2
			}
		},
	};

	s32Ret = CVI_VENC_CreateChn(ch, &venc_chn_attr);
	if (s32Ret != CVI_SUCCESS) {
		printf("CVI_VENC_CreateChn [%d] failed with %d\n", ch, s32Ret);
		return s32Ret;
	}

	SAMPLE_COMM_VPSS_Bind_VENC(0, 0, 0);

	VENC_RECV_PIC_PARAM_S stRecvParam;
	stRecvParam.s32RecvPicNum = -1;
	s32Ret = CVI_VENC_StartRecvFrame(ch, &stRecvParam);
	if (s32Ret != CVI_SUCCESS) {
		printf("CVI_VENC_StartRecvPic failed with %d\n", s32Ret);
		return CVI_FAILURE;
	}

	return s32Ret;
}

static void sys_venc_deinit(void)
{
	CVI_S32 s32Ret;
	SAMPLE_COMM_VPSS_UnBind_VENC(0, 0, 0);
	CVI_VENC_StopRecvFrame(0);
		s32Ret = CVI_VENC_ResetChn(0);
	if (s32Ret != CVI_SUCCESS) {
		CVI_VENC_ERR("CVI_VENC_ResetChn vechn[%d] failed with %#x!\n",
				0, s32Ret);
	}
	CVI_VENC_DestroyChn(0);
}

static int venc_save_stream_one(FILE *fp)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    VENC_CHN VencChn = 0;
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream = {0};
    //struct timespec ts;

    // 带超时的状态查询
    pthread_mutex_lock(&g_lock);
    if (g_stop_flag) {
        pthread_mutex_unlock(&g_lock);
        return CVI_FAILURE;
    }
    pthread_mutex_unlock(&g_lock);

    // 带超时的流获取（关键修改点）
    //clock_gettime(CLOCK_REALTIME, &ts);
    //ts.tv_sec += 1; // 1秒超时
    s32Ret = CVI_VENC_QueryStatus(VencChn, &stStat);
    if (s32Ret != CVI_SUCCESS) {
        printf("Query status failed: 0x%x\n", s32Ret);
        return s32Ret;
    }

    if (stStat.u32CurPacks == 0) {
        return CVI_SUCCESS;
    }

    stStream.pstPack = malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
    if (!stStream.pstPack) {
        printf("Memory allocation failed\n");
        return CVI_FAILURE;
    }

    // 使用带超时的获取流接口（关键修改点）, 1000ms 超时限制
    s32Ret = CVI_VENC_GetStream(VencChn, &stStream, 1000);
    if (s32Ret == CVI_ERR_VENC_BUSY) {
        printf("Get stream timeout, may be interrupted\n");
        free(stStream.pstPack);
        return CVI_SUCCESS; // 超时不视为错误
    }

    if (s32Ret != CVI_SUCCESS) {
        printf("Get stream failed: 0x%x\n", s32Ret);
        free(stStream.pstPack);
        return s32Ret;
    }

    // 写入文件（保持原子性操作）
    pthread_mutex_lock(&g_lock);
    if (!g_stop_flag) {
        for (CVI_U32 i = 0; i < stStream.u32PackCount; i++) {
            VENC_PACK_S *ppack = &stStream.pstPack[i];
            fwrite(ppack->pu8Addr + ppack->u32Offset,
                   ppack->u32Len - ppack->u32Offset, 1, fp);
        }
    }
    pthread_mutex_unlock(&g_lock);

    // 释放流资源
    s32Ret = CVI_VENC_ReleaseStream(VencChn, &stStream);
    if (s32Ret != CVI_SUCCESS) {
        printf("Release stream failed: 0x%x\n", s32Ret);
    }

    free(stStream.pstPack);
    return s32Ret;
}

// 修改venc_save_stream函数，确保及时关闭文件
#if 0
static int venc_save_stream(const char *filename, int framecnt)
{
    int s32Ret = 0;
    int venc_fd = CVI_VENC_GetFd(0);
    if (venc_fd <= 0) {
        printf("Venc failed to get fd %x\n", venc_fd);
        return venc_fd;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return -1;
    }

    while (framecnt-- > 0 && !g_stop_flag) {
		printf("[%s]11\n", __func__);
        s32Ret = venc_save_stream_one(fp);
		printf("[%s]22\n", __func__);
        if (s32Ret != CVI_SUCCESS || g_stop_flag) {
            printf("Stream saving interrupted\n");
            break;
        }
    }
	
	printf("[%s]33\n", __func__);
	if (g_stop_flag) {
        printf("[%s]Interrupted by signal, exiting...\n", __func__);
		s32Ret = CVI_FAILURE;
    }

	printf("[%s]44\n", __func__);
    fclose(fp);
	printf("[%s]55\n", __func__);
	
	printf("[%s]before return, value=%d\n", __func__, s32Ret);
	return s32Ret;
}
#endif

int main(int argc, char *argv[])
{
    if (check_arguments(argc, argv) != 0) return -1;

    // 阻塞目标信号，确保后续线程继承该设置
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGHUP);
	sigaddset(&sigset, SIGUSR1); 
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    pthread_mutex_init(&g_lock, NULL);
    pthread_cond_init(&g_cond, NULL);

    pthread_t tid;
    ThreadParams params = {
        argv[1], 
        atoi(argv[2]),
        pthread_self()  // 保存主线程ID
    };
	
    if (pthread_create(&tid, NULL, recording_thread, &params) != 0) {
        fprintf(stderr, "Thread creation failed\n");
        return EXIT_FAILURE;
    }

    // 主线程专责处理信号
	int sig;
	printf("Main thread waiting for signals...\n");
	int ret = sigwait(&sigset, &sig);
	if (ret == 0) {
		if (sig == SIGUSR1) {
			printf("Recording completed normally. Cleaning up resources...\n");
    } else {
        printf("\nCaught signal %d, initiating shutdown...\n", sig);
        pthread_mutex_lock(&g_lock);
        g_stop_flag = 1;
        pthread_cond_broadcast(&g_cond);
        pthread_mutex_unlock(&g_lock);
    }
	} else {
    fprintf(stderr, "sigwait error: %s\n", strerror(ret));
	}


    // 等待录像线程退出
    void *thread_ret;
    pthread_join(tid, &thread_ret);
    printf("Recording thread exited with code %ld\n", (long)thread_ret);

    // 执行全局资源清理
    sys_cleanup();

    pthread_mutex_destroy(&g_lock);
    pthread_cond_destroy(&g_cond);
    return EXIT_SUCCESS;
}