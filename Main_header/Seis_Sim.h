#pragma once
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

// =================================================================================
// 1. ƽ̨��ض�����ϵͳͷ�ļ� (Platform Specifics)
// =================================================================================
#ifdef _WIN32
#define NOMINMAX          // ���� Windows.h �Դ��� min/max �꣬������ std::min/max ��ͻ
#include <windows.h>      // Windows API
#include <direct.h>       // _mkdir
#include <io.h>           // _access
#else
#include <sys/stat.h>     // mkdir
#include <sys/types.h>
#include <unistd.h>
#endif

#include "Common.h"
#include "Cuda_Check.cuh" // CUDA ����ӿ�
#include "SeismicIO.h"

// �����ⲿδ�󶨵� FDM �������������ú���
extern "C" {
    // Creates the host-side simulation context from the selected 2D parameters.
    void setupTestContext(SimulationContext& ctx, const Parameters& par);
    // Generates the source time function for the requested sampling interval.
    void generateRickerWavelet(std::vector<float>& wavelet, int nt, float dt, float f0, float t0);
}

// =============================================================================
// 2. ȫ�ֳ־û�״̬���Ʊ��� (File-level Global State Variables)
// =============================================================================
static SimulationContext ctx;
static GPUSimData        gpu_data;
static bool              is_seis_initialized = false;

// ȫ�ֿ����ź�
inline bool g_resetSimRequested = false; // C ������ģ���ź�
inline bool g_resetViewportRequested = false; // R �������ӿ��ź�

// --- �����������С���� ---
inline int   edit_w = 1000;
inline int   edit_h = 500;
inline float edit_f0 = 100.0f;
inline float edit_t0 = 1.0f / edit_f0;

inline float temp_dt = 0.0002f;
inline float temp_dx = 1.0f;
inline int   temp_nt = 10000;
inline int   temp_pml = 50;

// --- ʱ���ݻ�����Ⱦ���� ---
inline int   current_it = 0;
inline float accumulated_compute_time = 0.0f;
inline float color_scale = 20.0f;
inline int   show_component = 2;     // 0: Vz, 1: Vx
inline int   waveStyle = 4;     // Ĭ�ϲ��� Style 7 (Turbo)
inline int   modelStyle = 1; // 0: �ѽ��, 1: ��ѧ����ͼ, 2: �Ҷ�Vp, 3: ���沨��, 4: ��ѧ�ױ���
inline int   steps_per_frame = 20;
inline bool  showHUD = true;
inline bool  show_Monitor_par = false;


// --- ������Դλ���������Ƕ� ---
inline int   edit_src_x = 500;
inline int   edit_src_z = 125;
inline float edit_angle = 0.0f;
inline float edit_amp = 1.0f; // ȫ�ֹ�����������ֵ���� (0.1 ~ 10.0x)
inline int   edit_source_type = 0;    // ȫ�ֹ�����������Դ���� (0:��ֱ��, 1:ˮƽ��, 2:����Դ, 3:����Դ, 4:��б��)

// --- �ӿ�ƽ����������� ---
inline float      viewZoom = 1.0f;
inline ImVec2     viewOffset = { 0.0f, 0.0f };
inline bool       first_align_needed = false;
inline Parameters par;

// --- ��̬����Դ������ģʽ ---
inline bool                  multi_source_mode = false;
inline std::vector<GPUSource> active_sources;
inline bool                  infinite_mode = false;

// --- �����ͣʵʱ������� ---
inline int   hover_mx = 0;
inline int   hover_my = 0;
inline float hover_Vp = 0.0f;
inline float hover_Vs = 0.0f;
inline float hover_Rho = 0.0f;
inline bool  hover_valid = false;

// --- ȫ�� UI ӫ������ɫ (Ĭ����ɫӫ��) ---
inline ImVec4 uiAccent = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);

inline bool is_mouse_inside = false;

inline int current_scene = SCENE_TWO_LAYER; // ��ǰ���ص���������

// --- �������Բ��� ---
inline float edit_Vp = 2000.0f; // ȫ�ֹ������ݲ��ٶ�
inline float edit_Vs = 1400.0f; // ȫ�ֹ������Შ�ٶ�
inline float edit_Density = 2000.0f; // ȫ�ֹ������ܶ�

inline GLuint g_modelTex = 0; // �������������������

inline bool  showGrid = true;  // Ĭ�Ͽ�������������������ʾ
inline float ui_gridSpacing = 10.0f; // �Ƽ�Ĭ�� 10.0f
inline float ui_gridOpacity = 1.0f;  // �Ƽ�Ĭ�� 1.0f (���� UI ���޼��䰵�����)

inline static bool        show_success_popup = false;
inline static bool        show_error_popup = false;
inline static std::string popup_message = "";

// --- �߼������������� (�ܿ�ÿ֡ѭ�� 400 ��㵼�µ� FPS ����ƿ��) ---
inline float cached_max_vp = 0.0f;
inline float cached_min_vp = 0.0f;
inline float cached_max_vs = 0.0f;
inline float cached_min_vs = 0.0f;
inline float cached_max_rho = 0.0f;
inline float cached_min_rho = 0.0f;
inline float cached_vram_mb = 0.0f; // �����Դ�ռ��

inline bool convert_rho=true;

inline float edit_segy_dx = 1.0f; // �ⲿ���� SEG-Y ʱʵ����������� (��)
inline float export_target_dx = 1.0f; // ����Ŀ�������� (�ף�Ĭ���뵱ǰģ�ⲽ������)

inline ReplayManager g_replay; // ȫ��Ψһ�Ĳ����طŹ���ʵ��

// =============================================================================
// �����ݲɼ����ġ����첨������λ��
// =============================================================================
struct ReceiverPos {
    int x;
    int y;
};

// =============================================================================
// �����ݲɼ����ġ������ݲɼ������¼�ƹ�����
// =============================================================================
struct AcquisitionManager {
    float samplingRateHz = 10000.0f; // ������
    float totalDurationSec = 0.5f;    // ¼����ʱ�� (��)
    float exportIntervalSec = 0.5f;
    int   numChannels = 100;     // ����
    int   startX = 100;     // ��� X ����
    int   endX = 900;     // �յ� X ����
    int   receiverDepth = 150;     // ����������� Z

    std::vector<ReceiverPos> receivers; // �첨������������������
    bool isShowarray = true;            // �Ƿ��ڲ�����ͼ�������ʾ�첨��
    bool isRecording = false;           // �Ƿ���¼��״̬

    float totalTimer = 0.0f;      // ¼�Ƽ�ʱ�� (��)
    float exportTimer = 0.0f;
    float timer = 0.0f;
    int   currentFileIndex = 1;
    int samplingStepStride = 1;
    float actualSampleInterval = 0.0f;

    // ��ά�ɼ����ݻ����� [channel][sample]
    std::vector<std::vector<float>> recorded_vx;
    std::vector<std::vector<float>> recorded_vz;

    // ��ʼ��/��ղɼ�������
    void initBuffer() {
        if (numChannels <= 0 || !std::isfinite(totalDurationSec) || totalDurationSec <= 0.0f) {
            throw std::invalid_argument("Recording requires positive duration and channel count");
        }
        samplingStepStride = getReceiverSamplingStride(ctx.dt, samplingRateHz);
        actualSampleInterval = samplingStepStride * ctx.dt;
        const double sampleCount = std::ceil(totalDurationSec / static_cast<double>(actualSampleInterval));
        if (!std::isfinite(sampleCount) || sampleCount > (std::numeric_limits<int>::max)()) {
            throw std::overflow_error("Recording sample count is too large");
        }
        int expectedPoints = std::max(1, static_cast<int>(sampleCount));
        recorded_vx.assign(numChannels, std::vector<float>(expectedPoints, 0.0f));
        recorded_vz.assign(numChannels, std::vector<float>(expectedPoints, 0.0f));
    }
};

// ȫ��Ψһ�Ĳɼ�������ʵ��
inline AcquisitionManager rec;

// =============================================================================
// �����ݲɼ����ġ���ȫ�Զ�����ѭ���ɼ�״̬��
// =============================================================================
enum AutoAcqState {
    AUTO_ACQ_IDLE = 0,
    AUTO_ACQ_RUNNING,
    AUTO_ACQ_FINISHED
};

// =============================================================================
// �����ݲɼ����ġ���ȫ�Զ�����ѭ���ɼ�״̬�� (�����棺֧�ֶ���������ȿ���)
// =============================================================================
struct AutoAcquisition {
    AutoAcqState state = AUTO_ACQ_IDLE;
    int          startX = 100;       // ��һ����ʼ X ��������
    int          shotDepthZ = 60;        // ���������������Զ����ڼ������ Z ��������
    int          shotIntervalX = 60;        // �ڼ�� (����)
    int          totalShots = 10;        // �ܲɼ�����
    int          currentShotIdx = 0;         // ��ǰ����¼�Ƶڼ���
    std::string  exportFolder = "exports/"; // �Զ������� SEG-Y �����ļ���

    void Start() {
        if (totalShots <= 0) return;
        state = AUTO_ACQ_RUNNING;
        currentShotIdx = 0;

        // ���������Զ������趨����������뼤�����
        edit_src_x = startX;
        edit_src_z = shotDepthZ;

        // =====================================================================
        // ��������������ȫ�Զ�ʱ����������� (CFL ������)
        //  ���ݲɼ�ʱ���Զ����㲢�޸� ctx.nt���� FDM �ݻ�������ɼ����� 100% �����Գƣ�
        // =====================================================================
        int required_steps = static_cast<int>(rec.totalDurationSec / ctx.dt);
        if (required_steps > 0) {
            ctx.nt = required_steps;
            temp_nt = required_steps; // ͬ����ǰ̨����

            // ���»��������������������Ĵ��Ӳ����飬��ֹ����Խ���Ƶ�ʻ���
            generateRickerWavelet(ctx.wavelet, ctx.nt, ctx.dt, edit_f0, edit_t0);
        }
    }
};

inline AutoAcquisition g_autoAcq; // ȫ��Ψһ�Ķ����Զ��ɼ�״̬��ʵ��


#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// =============================================================================
// ����ȫӲ����ء���C ���ݵ� NVML �ṹ�������Ͷ��� (������������� .h ���������ͻ)
// =============================================================================
typedef int nvmlReturn_t;
#define NVML_SUCCESS 0
#define NVML_TEMPERATURE_GPU 0

typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

typedef void* nvmlDevice_t;

// ����ָ�����Ͷ���
typedef nvmlReturn_t(*pfn_nvmlInit)();
typedef nvmlReturn_t(*pfn_nvmlShutdown)();
typedef nvmlReturn_t(*pfn_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t(*pfn_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t(*pfn_nvmlDeviceGetTemperature)(nvmlDevice_t, int, unsigned int*);
typedef nvmlReturn_t(*pfn_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);

// =============================================================================
// ����ȫӲ����ء�����̬������ NVML ������ (Symmetric GPU Loader)
// =============================================================================
struct DynamicNVML {
    bool loaded = false;
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif

    pfn_nvmlInit                     init = nullptr;
    pfn_nvmlShutdown                 shutdown = nullptr;
    pfn_nvmlDeviceGetHandleByIndex   getHandle = nullptr;
    pfn_nvmlDeviceGetUtilizationRates getUtil = nullptr;
    pfn_nvmlDeviceGetTemperature     getTemp = nullptr;
    pfn_nvmlDeviceGetPowerUsage      getPower = nullptr;

    nvmlDevice_t devHandle = nullptr;

    // ��̬����ϵͳ�� NVIDIA ��������
    void Load() {
        if (loaded) return;
#ifdef _WIN32
        handle = LoadLibraryA("nvml.dll"); // Windows ����Ŀ¼���Դ�
#else
        handle = dlopen("libnvidia-ml.so", RTLD_LAZY); // Linux ϵͳ��
#endif
        if (!handle) return;

#ifdef _WIN32
        init = (pfn_nvmlInit)GetProcAddress(handle, "nvmlInit");
        shutdown = (pfn_nvmlShutdown)GetProcAddress(handle, "nvmlShutdown");
        getHandle = (pfn_nvmlDeviceGetHandleByIndex)GetProcAddress(handle, "nvmlDeviceGetHandleByIndex");
        getUtil = (pfn_nvmlDeviceGetUtilizationRates)GetProcAddress(handle, "nvmlDeviceGetUtilizationRates");
        getTemp = (pfn_nvmlDeviceGetTemperature)GetProcAddress(handle, "nvmlDeviceGetTemperature");
        getPower = (pfn_nvmlDeviceGetPowerUsage)GetProcAddress(handle, "nvmlDeviceGetPowerUsage");
#else
        init = (pfn_nvmlInit)dlsym(handle, "nvmlInit");
        shutdown = (pfn_nvmlShutdown)dlsym(handle, "nvmlShutdown");
        getHandle = (pfn_nvmlDeviceGetHandleByIndex)dlsym(handle, "nvmlDeviceGetHandleByIndex");
        getUtil = (pfn_nvmlDeviceGetUtilizationRates)dlsym(handle, "nvmlDeviceGetUtilizationRates");
        getTemp = (pfn_nvmlDeviceGetTemperature)dlsym(handle, "nvmlDeviceGetTemperature");
        getPower = (pfn_nvmlDeviceGetPowerUsage)dlsym(handle, "nvmlDeviceGetPowerUsage");
#endif

        if (init && shutdown && getHandle && getUtil && getTemp && getPower) {
            if (init() == NVML_SUCCESS) {
                if (getHandle(0, &devHandle) == NVML_SUCCESS) {
                    loaded = true;
                }
                else {
                    shutdown();
                }
            }
        }
    }

    void Unload() {
        if (!loaded) return;
        if (shutdown) shutdown();
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        loaded = false;
    }
};

// ȫ��Ψһ�Ķ�̬Ӳ�������ʵ��
inline DynamicNVML g_nvml;

// =============================================================================
// 3. ���ļ���״̬�뼫ֵ���㻺������
// =============================================================================
// Recomputes cached model statistics after the underlying material grid changes.
inline void recalculateCachedModelMetrics() {
    if (ctx.total_grid <= 0) return;

    float max_p = -1e20f, min_p = 1e20f;
    float max_s = -1e20f, min_s = 1e20f;
    float max_r = -1e20f, min_r = 1e20f;

    for (int i = 0; i < ctx.total_grid; ++i) {
        float rho = ctx.rho[i];
        float vp = 0.0f;
        float vs = 0.0f;
        if (rho > 0.0f) {
            vp = sqrtf(ctx.lambda2mu[i] / rho);
            vs = sqrtf(ctx.mu[i] / rho);
        }

        // �ܿ���˵ĵ��ٿ����㣬����ʾ��ʵ�����Ҳ���ٶȷ�Χ
        if (vp > 350.0f) {
            if (vp > max_p) max_p = vp;
            if (vp < min_p) min_p = vp;
            if (vs > max_s) max_s = vs;
            if (vs < min_s) min_s = vs;
            if (rho > max_r) max_r = rho;
            if (rho < min_r) min_r = rho;
        }
    }

    // ��������
    if (max_p == -1e20f) {
        max_p = 2000.0f; min_p = 2000.0f;
        max_s = 1400.0f; min_s = 1400.0f;
        max_r = 2000.0f; min_r = 2000.0f;
    }

    cached_max_vp = max_p;
    cached_min_vp = min_p;
    cached_max_vs = max_s;
    cached_min_vs = min_s;
    cached_max_rho = max_r;
    cached_min_rho = min_r;

    // ���� GPU �ڴ˳ߴ��·�������� 2D ��������ϵ���Դ�
    // ����Ӧ�����ٶȡ�PML ���ѳ������Եȹ�Լ 20 �������ȶ�ά�����Դ��
    size_t estimated_bytes = ctx.total_grid * sizeof(float) * 20;
    cached_vram_mb = static_cast<float>(estimated_bytes) / (1024.0f * 1024.0f);
}


// =============================================================================
// 11. �����Ǿ��ȵ���������������ϴ���
// =============================================================================
// Rebuilds the visualization texture from the current model and graphics context.
inline void updateModelTexture() {
    if (g_modelTex == 0) {
        glGenTextures(1, &g_modelTex);
    }

    std::vector<float> temp_data(ctx.total_grid * 4, 0.0f);

    for (int i = 0; i < ctx.total_grid; ++i) {
        float rho = ctx.rho[i];
        float vp = 0.0f;
        float vs = 0.0f;
        if (rho > 0.0f) {
            vp = sqrtf(ctx.lambda2mu[i] / rho);
            vs = sqrtf(ctx.mu[i] / rho);
        }

        // ��һ��������Ӧͨ��
        temp_data[i * 4 + 0] = vp / 6000.0f;  // R ͨ��: �ݲ��ٶ� Vp (ӳ���� 6000m/s)
        temp_data[i * 4 + 1] = vs / 3500.0f;  // G ͨ��: �Შ�ٶ� Vs
        temp_data[i * 4 + 2] = rho / 3000.0f; // B ͨ��: �ܶ� Rho
        temp_data[i * 4 + 3] = 1.0f;          // A ͨ��
    }

    glBindTexture(GL_TEXTURE_2D, g_modelTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, ctx.NX, ctx.NZ, 0, GL_RGBA, GL_FLOAT, temp_data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}
// =============================================================================
// ��������������һ��ͬ�� CPU �ز��޸��� GPU ��ˢ�µ��ʱ�������ͼ��
// =============================================================================
// Uploads current material fields to the simulation's device buffers.
inline void uploadMaterialToGpu() {
    if (ctx.total_grid <= 0) return;

    size_t bytes = ctx.total_grid * sizeof(float);

    // 1. ���޸ĺ�Ļ���������÷�������ܶȾ������¿����Դ�
    CUDA_CHECK(cudaMemcpy(gpu_data.d_rho, ctx.rho.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(gpu_data.d_mu, ctx.mu.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(gpu_data.d_lambda, ctx.lambda.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(gpu_data.d_lambda2mu, ctx.lambda2mu.data(), bytes, cudaMemcpyHostToDevice));

    // 2. �����ǰ������ Type 2 PML (�Ƿ��ѻ��) �£������������鲢����ƽչ�İ벽���������Խ��ʳ� [3]
    if (gpu_data.flag_type == 2) {
        std::vector<float> mu_x(ctx.total_grid, 0.0f), lambda_x(ctx.total_grid, 0.0f), l2m_x(ctx.total_grid, 0.0f);
        std::vector<float> mu_z(ctx.total_grid, 0.0f), rho_xz(ctx.total_grid, 0.0f), rho_orig(ctx.total_grid, 0.0f);
        for (int i = 0; i < ctx.NZ; ++i) {
            for (int j = 0; j < ctx.NX; ++j) {
                int k = i * ctx.NX + j;
                rho_orig[k] = ctx.rho[k];
                int k_next_x = (j < ctx.NX - 1) ? k + 1 : k;
                int k_next_z = (i < ctx.NZ - 1) ? k + ctx.NX : k;
                mu_x[k] = 0.5f * (ctx.mu[k] + ctx.mu[k_next_x]);
                lambda_x[k] = 0.5f * (ctx.lambda[k] + ctx.lambda[k_next_x]);
                l2m_x[k] = 0.5f * (ctx.lambda2mu[k] + ctx.lambda2mu[k_next_x]);
                mu_z[k] = 0.5f * (ctx.mu[k] + ctx.mu[k_next_z]);
                int k_dr = (i < ctx.NZ - 1 && j < ctx.NX - 1) ? k + ctx.NX + 1 : k;
                rho_xz[k] = 0.25f * (ctx.rho[k] + ctx.rho[k_next_x] + ctx.rho[k_next_z] + ctx.rho[k_dr]);
            }
        }
        CUDA_CHECK(cudaMemcpy(gpu_data.d_mu_x_flat, mu_x.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(gpu_data.d_lambda_x_flat, lambda_x.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(gpu_data.d_lambda2mu_x_flat, l2m_x.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(gpu_data.d_mu_z_flat, mu_z.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(gpu_data.d_rho_x_z_flat, rho_xz.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(gpu_data.d_rho_orig_flat, rho_orig.data(), bytes, cudaMemcpyHostToDevice));
    }

    // 3. �����ǰ�� Type 3 PML (���ɱ���) �£����¼������ɱ��� dp_flat �迹
    if (gpu_data.flag_type == 3) {
        ctx.dp_flat.assign(ctx.NX, 0.0f);
        int fs_idx = ctx.npml;
        for (int j = 0; j < ctx.NX; ++j) {
            int k = fs_idx * ctx.NX + j;
            if (k < ctx.total_grid && ctx.lambda2mu[k] > 0.0f) {
                float l2m = ctx.lambda2mu[k];
                float lam = ctx.lambda[k];
                ctx.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
            }
        }
        CUDA_CHECK(cudaMemcpy(gpu_data.d_dp_flat, ctx.dp_flat.data(), ctx.NX * sizeof(float), cudaMemcpyHostToDevice));
    }

    // 4. ���� CPU ��������ֵ���Դ���㻺�� [3]
    recalculateCachedModelMetrics();

    // 5. �����ϴ���ˢ�����������ز�����ͼ�㣬ʹ OpenGL �ӿ�������ʾ�����ʻ����Ĳ�λ��ɫ��
    updateModelTexture();
}

// =============================================================================
// ���ع��桿���߱������������Ⱦ�� (������ĻĿ�����ؼ�෴������Ӧ����)
//  ֧�������ⳬ������߶������������ȡ������ص��س�������/��ǧ�궨����
// =============================================================================
inline void renderGridRulerOnBackbuffer(
    const SimulationContext& ctx,
    const SimState& state,
    int                      winW,
    int                      winH,
    float                    barHeight,
    const ImVec2& viewOffset,
    float                    viewZoom
) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImU32       textCol = IM_COL32(220, 220, 220, 225); // ӫ���
    ImU32       tickCol = IM_COL32(0, 255, 200, 100);   // ��ɫ�̶���

    float  winAspect = (float)winW / (float)winH;
    float  simAspect = (float)ctx.NX / (float)ctx.NZ;
    ImVec2 aspectCorr = { 1.0f, 1.0f };

    if (winAspect > simAspect) {
        aspectCorr.x = winAspect / simAspect;
    }
    else {
        aspectCorr.y = simAspect / winAspect;
    }

    // =========================================================================
    // ������������������ 100 ����Ŀ����� X / Y ��������������Ӧ������
    // =========================================================================
    auto calcNiceInterval = [&](float target_pixel_dist, float total_cells, float aspect_corr, float win_dim) -> float {
        // 1. ������ 100 �����ڵ�ǰ�������¶�Ӧ�������������� (��)
        float target_phys = (target_pixel_dist * total_cells * aspect_corr * ctx.h) / (win_dim * viewZoom);
        if (target_phys <= 0.0f) return 10.0f;

        // 2. ���� Log10 �������뵽 10 �� N ���ݻ�׼
        float log_val = log10f(target_phys);
        float base_power = powf(10.0f, floorf(log_val));
        float ratio = target_phys / base_power;

        // 3. ���ձ�׼��ѧ��ͼ������� (1, 2, 5 ���ַ�) ƥ�������۵Ĳ���
        float nice_interval = base_power;
        if (ratio > 5.0f)      nice_interval = base_power * 5.0f;
        else if (ratio > 2.0f) nice_interval = base_power * 2.0f;

        return (nice_interval < 1.0f) ? 1.0f : nice_interval;
        };

    // �ֱ������� X ��� Y �����طֱ��ʵ������������ (��λ����)
    float nice_interval_x = calcNiceInterval(100.0f, ctx.NX, aspectCorr.x, (float)winW);
    float nice_interval_y = calcNiceInterval(100.0f, ctx.NZ, aspectCorr.y, (float)winH);

    // =========================================================================
    // A. ���ƶ��� X ������Ӧ�����̶� (���������ס�Ϊѭ����λ����֤�̶�ֵ��Զ������/��ʮ)
    // =========================================================================
    float total_width_m = ctx.NX * ctx.h;
    for (float physX = 0.0f; physX <= total_width_m; physX += nice_interval_x) {
        // ������λ�÷�������Ϊ������������� NDC �任������ͶӰ
        float simX = physX / ctx.h;
        float aPos_x = (simX / (float)ctx.NX) * 2.0f - 1.0f;
        float Pndc_x = (aPos_x - viewOffset.x) * viewZoom / aspectCorr.x;
        float screenX = (Pndc_x * 0.5f + 0.5f) * winW;

        if (screenX >= 0.0f && screenX <= (float)winW) {
            float screenY = barHeight + 2.0f;
            draw_list->AddLine(ImVec2(screenX, screenY), ImVec2(screenX, screenY + 6.0f), tickCol, 1.5f);

            char buf[32];
            snprintf(buf, 32, "%.0fm", physX);

            ImVec2 txtSize = ImGui::CalcTextSize(buf);
            float  offsetX = -txtSize.x * 0.5f;

            ImVec2 pos(screenX + offsetX, screenY + 8.0f);
            draw_list->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), buf);
            draw_list->AddText(pos, textCol, buf);
        }
    }

    // =========================================================================
    // B. ������� Y ������Ӧ��ȿ̶� (���������ס�Ϊѭ����λ)
    // =============================================================================
    float total_depth_m = ctx.NZ * ctx.h;
    for (float physY = 0.0f; physY <= total_depth_m; physY += nice_interval_y) {
        float simY = physY / ctx.h;
        float aPos_y = 1.0f - (simY / (float)ctx.NZ) * 2.0f;
        float Pndc_y = (aPos_y - viewOffset.y) * viewZoom / aspectCorr.y;
        float screenY = (0.5f - Pndc_y * 0.5f) * winH;

        if (screenY >= barHeight && screenY <= (float)winH) {
            float screenX = 4.0f;
            draw_list->AddLine(ImVec2(screenX, screenY), ImVec2(screenX + 6.0f, screenY), tickCol, 1.5f);

            char buf[32];
            snprintf(buf, 32, "%.0fm", physY);

            ImVec2 txtSize = ImGui::CalcTextSize(buf);
            ImVec2 pos(screenX + 8.0f, screenY - txtSize.y * 0.5f);

            draw_list->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), buf);
            draw_list->AddText(pos, textCol, buf);
        }
    }
}

// =============================================================================
// 5. ��껭�ʹ����Ⱦ (�ǿ�/�Ƽ���������Ӧ)
// =============================================================================
inline void renderBrushCursor(const SimState& state, const ViewportInfo& vp) {
    ImVec2 mousePos = ImGui::GetMousePos();
    bool   isInside = (mousePos.x >= vp.x && mousePos.x < (vp.x + vp.w) &&
        mousePos.y >= vp.y && mousePos.y < (vp.y + vp.h));

    if (ImGui::GetIO().WantCaptureMouse) return;

    if (isInside) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();

        if (state.brushType == TOOL_NONE) {
            float t = (float)ImGui::GetTime();
            const float crosshairSize = 18.0f;
            const float outerRingRadius = 35.0f;

            // -----------------------------------------------------------------
            // ������Ӧɫ��ϵͳ�����⹹ȫ�� uiAccent ����ɫ���и�������ӳ��
            // -----------------------------------------------------------------
            float r = uiAccent.x;
            float g = uiAccent.y;
            float b = uiAccent.z;

            // ��ɫ������ǰ̨���� UI 100% ��������
            ImU32 col_main = IM_COL32(r * 255, g * 255, b * 255, 255);

            // ��̬ɨ��ָ��ɫ��������ɫ�����Ͻ��и����Ƹߣ��γ�ҫ�۵ķ����
            ImU32 col_dynamic = IM_COL32(
                (int)std::min(255.0f, r * 1.15f * 255.0f),
                (int)std::min(255.0f, g * 1.15f * 255.0f),
                (int)std::min(255.0f, b * 1.15f * 255.0f),
                255
            );

            // ͶӰ��Ӱ���޸�Ϊ��Э�������/���̰��ǣ�ʹ�̹��Ե�Աȸ����
            ImU32 col_shadow = IM_COL32(10, 18, 12, 160);

            // -----------------------------------------------------------------
            // ͼ������߼� (����ԭ�и����ܼ��νṹ)
            // -----------------------------------------------------------------
            // 1. �ײ�߶Ա�ͶӰʮ����
            fg->AddLine(ImVec2(mousePos.x - crosshairSize, mousePos.y), ImVec2(mousePos.x + crosshairSize, mousePos.y), col_shadow, 4.0f);
            fg->AddLine(ImVec2(mousePos.x, mousePos.y - crosshairSize), ImVec2(mousePos.x, mousePos.y + crosshairSize), col_shadow, 4.0f);

            // 2. ��������ɫʮ��׼��
            fg->AddLine(ImVec2(mousePos.x - crosshairSize, mousePos.y), ImVec2(mousePos.x + crosshairSize, mousePos.y), col_main, 2.0f);
            fg->AddLine(ImVec2(mousePos.x, mousePos.y - crosshairSize), ImVec2(mousePos.x, mousePos.y + crosshairSize), col_main, 2.0f);

            // 3. �Ƽ�׼�����Բ��
            fg->AddCircle(mousePos, outerRingRadius, col_main, 32, 1.5f);

            // 4. �״���תɨ��ɨ����
            float  angle = t * 4.0f;
            ImVec2 scanEnd = ImVec2(mousePos.x + cos(angle) * outerRingRadius,
                mousePos.y + sin(angle) * outerRingRadius);
            fg->AddLine(mousePos, scanEnd, col_dynamic, 2.0f);
            fg->AddCircleFilled(scanEnd, 3.5f, col_dynamic);

            // 5. ���������������岨
            float pulse_t = fmodf(t, 1.5f) / 1.5f;
            float pulse_radius = pulse_t * outerRingRadius;
            int   pulse_alpha = (int)(sin(pulse_t * 3.14159f) * 120);

            // ���岨��ɫͬ��ʹ�ô�͸���ȵ�����ɫ
            ImU32 col_pulse = IM_COL32(r * 255, g * 255, b * 255, pulse_alpha);
            fg->AddCircle(mousePos, pulse_radius, col_pulse, 32, 3.0f);
        }
        else {
            ImU32 brushColor;
            switch (state.brushType) {
            case TOOL_HIGH:   brushColor = IM_COL32(255, 100, 100, 200); break;
            case TOOL_LOW:    brushColor = IM_COL32(100, 150, 255, 200); break;
            case TOOL_WALL:   brushColor = IM_COL32(255, 255, 0, 200);   break;
            case TOOL_ERASER: brushColor = IM_COL32(200, 200, 200, 200); break;
            default:          brushColor = IM_COL32(255, 255, 255, 200); break;
            }

            float screenRadius = state.brushRadius * vp.scaleX;
            fg->AddCircle(mousePos, screenRadius, brushColor, 0, 2.0f);
            fg->AddCircle(mousePos, screenRadius - 1.0f, IM_COL32(0, 0, 0, 150), 0, 1.0f);
            fg->AddCircleFilled(mousePos, 3.0f, IM_COL32(255, 255, 255, 255));
        }
    }
}

// =============================================================================
// 6. ������Դ׼����Ⱦ (�߾����汳��ƽ�����š���Ư��)
// =============================================================================
inline void renderSourceMarker(
    const SimulationContext& ctx,
    int                      src_x,
    int                      src_z,
    int                      winW,
    int                      winH,
    float                    barHeight,
    const ImVec2& viewOffset,
    float                    viewZoom
) {
    float  winAspect = (float)winW / (float)winH;
    float  simAspect = (float)ctx.NX / (float)ctx.NZ;
    ImVec2 aspectCorr = { 1.0f, 1.0f };

    if (winAspect > simAspect) {
        aspectCorr.x = winAspect / simAspect;
    }
    else {
        aspectCorr.y = simAspect / winAspect;
    }

    float aPos_x = ((float)src_x / (float)ctx.NX) * 2.0f - 1.0f;
    float aPos_y = 1.0f - ((float)src_z / (float)ctx.NZ) * 2.0f;

    float Pndc_x = (aPos_x - viewOffset.x) * viewZoom / aspectCorr.x;
    float screenX = (Pndc_x * 0.5f + 0.5f) * winW;

    float Pndc_y = (aPos_y - viewOffset.y) * viewZoom / aspectCorr.y;
    float screenY = (0.5f - Pndc_y * 0.5f) * winH;

    if (screenX >= 0.0f && screenX <= winW && screenY >= barHeight && screenY <= winH) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        ImVec2      pos(screenX, screenY);

        ImU32 colMain = IM_COL32(125, 255, 125, 255);
        ImU32 colBlack = IM_COL32(0, 0, 0, 180);
        float lineLen = 20.0f;
        float gap = 3.0f;

        fg->AddLine(ImVec2(pos.x - lineLen, pos.y), ImVec2(pos.x + lineLen, pos.y), colBlack, 3.5f);
        fg->AddLine(ImVec2(pos.x, pos.y - lineLen), ImVec2(pos.x, pos.y + lineLen), colBlack, 3.5f);

        fg->AddLine(ImVec2(pos.x - lineLen, pos.y), ImVec2(pos.x - gap, pos.y), colMain, 1.5f);
        fg->AddLine(ImVec2(pos.x + gap, pos.y), ImVec2(pos.x + lineLen, pos.y), colMain, 1.5f);
        fg->AddLine(ImVec2(pos.x, pos.y - lineLen), ImVec2(pos.x, pos.y - gap), colMain, 1.5f);
        fg->AddLine(ImVec2(pos.x, pos.y + gap), ImVec2(pos.x, pos.y + lineLen), colMain, 1.5f);

        char buf[64];
        snprintf(buf, 64, "%d, %d (%.0fm, %.0fm)", src_x, src_z, src_x * ctx.h, src_z * ctx.h);

        ImVec2 textPos = ImVec2(pos.x + 8, pos.y + 8);
        fg->AddText(ImVec2(textPos.x + 1, textPos.y + 1), colBlack, buf);
        fg->AddText(textPos, colMain, buf);

        float t = (float)ImGui::GetTime();
        float pulse = (sin(t * 3.0f) * 0.5f + 0.5f) * 5.0f;
        fg->AddCircle(pos, 5.0f + pulse, IM_COL32(125, 255, 125, 155), 16, 1.0f);
    }
}
// =============================================================================
// ����������ȫ������ģ̬������Ⱦ�� (�������سɹ�/ʧ�ܵ� UI ��������)
// =============================================================================
inline void renderSeisPopups() {
    // -------------------------------------------------------------------------
    // A. �ɹ�ģ̬���� (�̿����)
    // -------------------------------------------------------------------------
    if (show_success_popup) {
        ImGui::OpenPopup("Simulation Success");
        show_success_popup = false; // ���ѵ��μ����źţ������ظ� Open
    }

    // Ϊ�ɹ���������ɫӫ��߿�
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    if (ImGui::BeginPopupModal("Simulation Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[ SUCCESS ]");
        ImGui::Separator();
        ImGui::Spacing();

        // ��Ⱦ�����ں�̨д����ַ�����Ϣ
        ImGui::Text("%s", popup_message.c_str());

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // -------------------------------------------------------------------------
    // B. ʧ��ģ̬���� (������)
    // -------------------------------------------------------------------------
    if (show_error_popup) {
        ImGui::OpenPopup("Simulation Error");
        show_error_popup = false; // ���ѵ��μ����ź�
    }

    // Ϊʧ�ܵ����󶨸߾�ʾ�Ⱥ��
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    if (ImGui::BeginPopupModal("Simulation Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[ ERROR ]");
        ImGui::Separator();
        ImGui::Spacing();

        // ��Ⱦ��̨д��Ĵ��������Ϣ
        ImGui::Text("%s", popup_message.c_str());

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// �򵥵��׿��Ӳ�������
void generateRickerWavelet(std::vector<float>& wavelet, int nt, float dt, float f0, float t0) {
    if (nt <= 0 || !std::isfinite(dt) || dt <= 0.0f || !std::isfinite(f0) || f0 <= 0.0f || !std::isfinite(t0)) {
        throw std::invalid_argument("Invalid Ricker wavelet sampling or frequency");
    }
    wavelet.resize(nt);
    for (int i = 0; i < nt; ++i) {
        float t = i * dt - t0;
        float pi2_f0 = M_PI * M_PI * f0 * f0;
        wavelet[i] = (1.0f - 2.0f * pi2_f0 * t * t) * expf(-pi2_f0 * t * t);
    }
}


/// //////////////////////////////////////////�ڶ�����
// =============================================================================
// �������������ݵ�ǰ�ز��������ޣ�һ���Զ����벢���������ȫʱ�䲽�� (CFL = 0.480)
//  ÿ���л�Ԥ�衢���� SEG-Y/TXT �Զ���ģ��ʱ��һ�����ô˺������������������
// =============================================================================
// =============================================================================
// �������󡿣�һ���Զ����벢���������ȫʱ�䲽�� (CFL = 0.480)
// =============================================================================
/** Clamps the time step to the stability limit derived from the current grid and maximum velocity. */
inline void autoAlignTimeStep() {
    if (ctx.total_grid <= 0) return;

    // 1. �Զ���ȡ�ز�����ݲ��ٶ�
    float max_vp = 0.0f;
    for (int k = 0; k < ctx.total_grid; ++k) {
        float vp = sqrtf(ctx.lambda2mu[k] / ctx.rho[k]);
        if (vp > max_vp) max_vp = vp;
    }
    if (max_vp <= 350.0f) max_vp = 2000.0f;

    // 2. �Զ������ڼ��ް�ȫ����
    ctx.dt = kSafeCourant2d * ctx.h / max_vp;
    gpu_data.dt = ctx.dt;
    gpu_data.inv_dt = 1.0f / ctx.dt;
    temp_dt = ctx.dt;

    // =========================================================================
    // �������޸���������ʹ�����¡�������ʱ�䲽�� ctx.dt �������� Ricker �Ӳ���
    //  �����þ� dt ���ɵ��Ӳ�����ʱ���²��Żᷢ�����ص�Ƶ�ʻ��� (��������)
    // =========================================================================
    generateRickerWavelet(ctx.wavelet, ctx.nt, ctx.dt, edit_f0, edit_t0);
}
// =============================================================================
// 7. PML ���ղ�����˥��ϵ������ (רע�ڲ��ϵ����������ԽȨ�޸�ϵͳȫ�� dt)
// =============================================================================
/** Rebuilds staggered-grid PML damping profiles from the current model and boundary settings. */
inline void updatePmlDampingArrays() {
    validateBoundaryGeometry(ctx.NX, ctx.NZ, ctx.npml, ctx.flag_type == 3 && ctx.upFlag);
    float max_vp = 0.0f;
    for (int k = 0; k < ctx.total_grid; ++k) {
        float vp = sqrtf(ctx.lambda2mu[k] / ctx.rho[k]);
        if (vp > max_vp) max_vp = vp;
    }
    if (max_vp <= 350.0f) max_vp = 2000.0f;

    ctx.dx.assign(ctx.NX, 0.0f);
    ctx.dx_half.assign(ctx.NX, 0.0f);
    ctx.dz.assign(ctx.NZ, 0.0f);
    ctx.dz_half.assign(ctx.NZ, 0.0f);

    if (ctx.npml > 0) {
        float L = ctx.npml * ctx.h;
        float d_max = (3.0f * max_vp * 16.12f * 3.5f) / (2.0f * L);
        float pml_power = 2.5f;

        for (int i = 0; i < ctx.npml; ++i) {
            float x_thick = (ctx.npml - i) / (float)ctx.npml;
            float x_thick_half = (ctx.npml - (i + 0.5f)) / (float)ctx.npml;
            if (x_thick_half < 0.0f) x_thick_half = 0.0f;

            float val = d_max * powf(x_thick, pml_power);
            float val_half = d_max * powf(x_thick_half, pml_power);

            ctx.dx[i] = val;
            ctx.dx[ctx.NX - 1 - i] = val;
            ctx.dx_half[i] = val_half;
            ctx.dx_half[ctx.NX - 1 - i] = val_half;
        }

        for (int i = 0; i < ctx.npml; ++i) {
            float z_thick = (ctx.npml - i) / (float)ctx.npml;
            float z_thick_half = (ctx.npml - (i + 0.5f)) / (float)ctx.npml;
            if (z_thick_half < 0.0f) z_thick_half = 0.0f;

            float val = d_max * powf(z_thick, pml_power);
            float val_half = d_max * powf(z_thick_half, pml_power);

            ctx.dz[i] = val;
            ctx.dz[ctx.NZ - 1 - i] = val;
            ctx.dz_half[i] = val_half;
            ctx.dz_half[ctx.NZ - 1 - i] = val_half;
        }
    }
}

// =============================================================================
// 8. ������������������Ĳ�������
// =============================================================================
void setupTestContext(SimulationContext& ctx, const Parameters& par) {
    ctx.NX = par.model.xnum;
    ctx.NZ = par.model.znum;
    ctx.total_grid = static_cast<int>(checkedGridElementCount(ctx.NX, ctx.NZ));
    ctx.dt = par.FDM.dt;
    ctx.nt = static_cast<int>(par.FDM.nt);
    ctx.npml = static_cast<int>(par.FDM.npml);

    // �����״�����δ��ʼ��ʱ��ΪĬ��ֵ 3����������̳��û��� UI �ϵ�ѡ��
    if (ctx.flag_type == 0) {
        ctx.flag_type = 3;
    }

    ctx.upFlag = (par.FDM.upFlag > 0.5f);

    float h = par.model.dx;
    if (h <= 0.0f) h = 1.0f; // ��������
    ctx.h = h;

    temp_dx = h;
    temp_dt = ctx.dt;
    temp_nt = ctx.nt;
    temp_pml = ctx.npml;

    // ���޵�λ�� 8 �����޲��ϵ�����Կռ䲽�� h 
    ctx.c1_h = static_cast<float>(kStaggeredDerivativeWeights[0]) / h;
    ctx.c2_h = static_cast<float>(kStaggeredDerivativeWeights[1]) / h;
    ctx.c3_h = static_cast<float>(kStaggeredDerivativeWeights[2]) / h;
    ctx.c4_h = static_cast<float>(kStaggeredDerivativeWeights[3]) / h;

    // ֱ�Ӷ�ȡ������ͷ�ļ�����ġ��� UI ǿ�󶨵�ȫ�ֹ������Ա���
    float mu_val = edit_Density * edit_Vs * edit_Vs;
    float lambda2mu_val = edit_Density * edit_Vp * edit_Vp;
    float lambda_val = lambda2mu_val - 2.0f * mu_val;

    // ������䱾�� Context
    ctx.rho.assign(ctx.total_grid, edit_Density);
    ctx.mu.assign(ctx.total_grid, mu_val);
    ctx.lambda.assign(ctx.total_grid, lambda_val);
    ctx.lambda2mu.assign(ctx.total_grid, lambda2mu_val);

    // ���¼��� PML ����������
    updatePmlDampingArrays();

    // ���ɱ��� dp_flat ����
    {
        ctx.dp_flat.assign(ctx.NX, 0.0f);
        int fs_idx = ctx.npml;
        for (int j = 0; j < ctx.NX; ++j) {
            int k = fs_idx * ctx.NX + j;
            if (k < ctx.total_grid && ctx.lambda2mu[k] > 0.0f) {
                float l2m = ctx.lambda2mu[k];
                float lam = ctx.lambda[k];
                ctx.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
            }
        }
    }

    // ��Դ��ʼ���趨
    ctx.src_z_idx = ctx.NZ / 4;                                      // Z ������趨�� 1/4 ��
    ctx.src_idx = ctx.src_z_idx * ctx.NX + (ctx.NX / 2);             // X �ᶨλ������ 1/2 ��
    ctx.src_angle = par.FDM.angle;
    ctx.src_type = edit_source_type; // ������
    ctx.src_amp = edit_amp;         // �󶨷�ֵ
    generateRickerWavelet(ctx.wavelet, ctx.nt, ctx.dt, par.FDM.f0, par.FDM.t0);

    // �첨�� (�м�ˮƽ��һ�м첨��)
    ctx.num_rcv = std::min(100, ctx.NX - 2 * kStencilRadius);
    ctx.rcv_grid_idx.resize(ctx.num_rcv);
    int rcv_z = std::min(ctx.NZ / 2 + 30, ctx.NZ - kStencilRadius - 1);
    int step = std::max(1, (ctx.NX - 2 * kStencilRadius) / ctx.num_rcv);
    for (int r = 0; r < ctx.num_rcv; ++r) {
        ctx.rcv_grid_idx[r] = rcv_z * ctx.NX + kStencilRadius + (r * step);
    }

    ctx.record_vx.assign(static_cast<size_t>(ctx.num_rcv) * ctx.nt, 0.0f);
    ctx.record_vz.assign(static_cast<size_t>(ctx.num_rcv) * ctx.nt, 0.0f);

    recalculateCachedModelMetrics();
}

// =============================================================================
// 9. ��������ע���������ٶȡ��ܶ�ת��Ϊ CPU �˵�����÷���� (���� Y ��)
// =============================================================================
/** Writes one cell's elastic properties, converting velocity and density to Lamé parameters. */
inline void setMaterialAt(int x, int y, float vp, float vs, float rho) {
    // ��ֱ������룺ʹ loadScenario �� y=0 ��Ӧ CUDA �������ײ� (NZ - 1 - y)
    int k = (ctx.NZ - 1 - y) * ctx.NX + x;
    if (k >= 0 && k < ctx.total_grid) {
        ctx.rho[k] = rho;
        ctx.mu[k] = rho * vs * vs;
        ctx.lambda2mu[k] = rho * vp * vp;
        ctx.lambda[k] = ctx.lambda2mu[k] - 2.0f * ctx.mu[k];
    }
}

// =============================================================================
// 10. ����װ�������������� 11 �׸�ά�����������泡��
// =============================================================================
/** Fills the current 2D material grid with a selected built-in geological test scenario. */
inline void loadScenario(int type) {
    int W = ctx.NX;
    int H = ctx.NZ;

    // Ĭ�ϱ������� (Ӳɰ��)
    float defVp = 3000.0f, defVs = 1732.0f, defRho = 2200.0f;

    // Ĭ�ϱ������
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            setMaterialAt(x, y, defVp, defVs, defRho);
        }
    }

    // Ĭ�ϼ���λ���ض�λ
    edit_src_x = W / 2;
    edit_src_z = H / 2;

    if (type == SCENE_EARTH) {
        // �غ˵�᣷ֲ�ģ�� (չʾҺ̬����˺Შ Vs=0 ��ʧ�� P ��͸����������)
        int   cx = W / 2;
        int   cy = H / 2;
        float maxRadius = (std::min(W, H) / 2.0f) * 0.95f;
        float r_core = maxRadius * 0.54f;
        float r_mantle = maxRadius * 0.92f;

        float r_core_sq = r_core * r_core;
        float r_mantle_sq = r_mantle * r_mantle;
        float max_sq = maxRadius * maxRadius;

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float dx = (float)(x - cx);
                float dy = (float)(y - cy);
                float distSq = dx * dx + dy * dy;

                if (distSq < r_core_sq) {
                    setMaterialAt(x, y, 4000.0f, 0.0f, 4000.0f);    // Һ̬�غˣ�Vs=0
                }
                else if (distSq < r_mantle_sq) {
                    setMaterialAt(x, y, 6000.0f, 3400.0f, 3000.0f); // ������
                }
                else if (distSq < max_sq) {
                    setMaterialAt(x, y, 3000.0f, 1700.0f, 2500.0f); // �ؿ�
                }
                else {
                    setMaterialAt(x, y, 340.0f, 10.0f, 100.0f);     // �ⲿ���ٽ���
                }
            }
        }
        edit_src_x = W / 2;
        edit_src_z = H / 2;
    }
    else if (type == SCENE_DOUBLE_SLIT) {
        // ˮ�и��迹˫�쵲����泡��
        int cy = H / 2;
        int cx = W / 2;
        int wallX = cx;

        // ˮ���ʱ��� (Vp=1500, Vs=0)
        for (int i = 0; i < W * H; i++) {
            setMaterialAt(i % W, i / W, 1500.0f, 0.0f, 1000.0f);
        }

        // ���迹ǽ������ (��10��) ��˫��
        for (int y = 0; y < H; ++y) {
            bool isSlit = (abs(y - cy) > 30 && abs(y - cy) < 60);
            if (!isSlit) {
                for (int w = 0; w < 10; w++) {
                    setMaterialAt(wallX + w, y, 5500.0f, 3200.0f, 8000.0f); // �������ܸ�������
                }
            }
        }
        edit_src_x = W / 4;
        edit_src_z = cy;
    }
    else if (type == SCENE_TWO_LAYER) {
        // ����˫���״����ģ��
        int interfaceY = H / 2;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                if (y < interfaceY) {
                    setMaterialAt(x, y, 4000.0f, 2300.0f, 2500.0f); // �Ӳ��
                }
                else {
                    setMaterialAt(x, y, 2000.0f, 1100.0f, 1500.0f); // ǳ������
                }
            }
        }
        edit_src_x = W / 2;
        edit_src_z = H / 2 + 50;
    }
    else if (type == SCENE_WAVEGUIDE) {
        // ֱ��״���ٲ���ͨ�� (����������ȫ�������ƴ���)
        int cy = H / 2;
        int guideHalfWidth = 60;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                if (abs(y - cy) < guideHalfWidth) {
                    setMaterialAt(x, y, 2000.0f, 1000.0f, 1500.0f); // ����о��
                }
                else {
                    setMaterialAt(x, y, 100.0f, 60.0f, 10000.0f);   // ���迹����
                }
            }
        }
        edit_src_x = 100;
        edit_src_z = cy;
    }
    else if (type == SCENE_WAVEGUIDE_CURVED) {
        // �����������������ٲ���
        int cy = H / 2;
        int guideHalfWidth = 60;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float currentCenterY = (H / 2) + (H / 4) * sin((float)x / W * 2.0f * 3.14159f);
                if (abs(y - currentCenterY) < guideHalfWidth) {
                    setMaterialAt(x, y, 2000.0f, 1000.0f, 1500.0f);
                }
                else {
                    setMaterialAt(x, y, 5500.0f, 3200.0f, 3000.0f);
                }
            }
        }
        edit_src_x = 100;
        edit_src_z = H / 2;
    }
    else if (type == SCENE_CRYSTAL) {
        // ���彻���������Ӿ��� (�����դ�谭����沨ǰ)
        float bgVp = 1500.0f, bgVs = 0.0f, bgRho = 1000.0f;   // ˮ����
        float scVp = 5800.0f, scVs = 3200.0f, scRho = 7800.0f;// ����
        int   radius = 12, spacingX = 50, spacingY = 50;
        int   startX = 250, endX = W - 100, startY = 100, endY = H - 100;

        for (int i = 0; i < W * H; ++i) setMaterialAt(i % W, i / W, bgVp, bgVs, bgRho);

        int   cols = (endX - startX) / spacingX;
        int   rows = (endY - startY) / spacingY;
        float r2 = (float)(radius * radius);

        for (int j = 0; j < rows; ++j) {
            for (int i = 0; i < cols; ++i) {
                int cx = startX + i * spacingX;
                int cy = startY + j * spacingY;
                if (j % 2 == 1) cx += spacingX / 2; // ���ѽ���

                for (int y = cy - radius; y <= cy + radius; ++y) {
                    for (int x = cx - radius; x <= cx + radius; ++x) {
                        if (x >= 0 && x < W && y >= 0 && y < H) {
                            float dx = (float)(x - cx);
                            float dy = (float)(y - cy);
                            if (dx * dx + dy * dy <= r2) {
                                setMaterialAt(x, y, scVp, scVs, scRho);
                            }
                        }
                    }
                }
            }
        }
        edit_src_x = 100;
        edit_src_z = H / 2;
    }
    else if (type == SCENE_RANDOM_SCATTER) {
        // ������ݽ���ǿɢ�䳡
        float bgVp = 2500.0f, bgVs = 1200.0f, bgRho = 2000.0f;
        float scVp = 4500.0f, scVs = 2500.0f, scRho = 2800.0f;

        for (int i = 0; i < W * H; ++i) setMaterialAt(i % W, i / W, bgVp, bgVs, bgRho);

        int numScatterers = 300;
        int minR = 5, maxR = 15;
        for (int k = 0; k < numScatterers; ++k) {
            int   cx = rand() % (W - 100) + 50;
            int   cy = rand() % (H - 100) + 50;
            int   r = rand() % (maxR - minR) + minR;
            float r2 = (float)(r * r);

            for (int y = cy - r; y <= cy + r; ++y) {
                for (int x = cx - r; x <= cx + r; ++x) {
                    if (x >= 0 && x < W && y >= 0 && y < H) {
                        float dx = x - cx;
                        float dy = y - cy;
                        if (dx * dx + dy * dy <= r2) {
                            setMaterialAt(x, y, scVp, scVs, scRho);
                        }
                    }
                }
            }
        }
        edit_src_x = W / 2;
        edit_src_z = H / 2;
    }
    else if (type == SCENE_CURVED) {
        // �������������ʷֽ���
        float l1_Vp = 2000.0f, l1_Vs = 1100.0f, l1_Rho = 1800.0f;
        float l2_Vp = 3500.0f, l2_Vs = 2000.0f, l2_Rho = 2400.0f;
        float l3_Vp = 5500.0f, l3_Vs = 3200.0f, l3_Rho = 2800.0f;

        float baseH1 = H * 0.35f, amp1 = H * 0.08f, freq1 = 2.0f * 3.14159f / W * 2.5f;
        float baseH2 = H * 0.65f, amp2 = H * 0.05f, freq2 = 2.0f * 3.14159f / W * 1.5f, phase2 = 1.0f;

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float h1 = baseH1 + amp1 * sin(x * freq1);
                float h2 = baseH2 + amp2 * sin(x * freq2 + phase2);
                if (y < h1)       setMaterialAt(x, y, l3_Vp, l3_Vs, l3_Rho);
                else if (y < h2)  setMaterialAt(x, y, l2_Vp, l2_Vs, l2_Rho);
                else              setMaterialAt(x, y, l1_Vp, l1_Vs, l1_Rho);
            }
        }
        edit_src_x = W / 2;
        edit_src_z = H - 50;
    }
    else if (type == SCENE_REFRACTION) {
        // �ٶ������������ӵ��ݶȽ��� (����ͻ�ת�ص���)
        float vTop = 1000.0f, vBot = 2000.0f;
        float rhoTop = 1800.0f, rhoBot = 2500.0f;
        for (int y = 0; y < H; ++y) {
            float ratio = (float)y / (float)H;
            float vp = vBot + (vTop - vBot) * ratio;
            float vs = vp / 1.732f;
            if (vp < 1000.0f) vs = 0.0f;
            float rho = rhoBot + (rhoTop - rhoBot) * ratio;

            for (int x = 0; x < W; ++x) setMaterialAt(x, y, vp, vs, rho);
        }
        edit_src_x = 100;
        edit_src_z = H - 50; // ǳ�㼤��
    }
    else if (type == SCENE_PENROSE_ROOM) {
        // ����˹��Բ���� (չʾ����˫������͸��ЧӦ)
        float rockVp = 3000.0f, rockVs = 1732.0f, rockRho = 2500.0f;
        float airVp = 0.0f, airVs = 0.0f, airRho = 20.0f;
        float cx = W * 0.5f, cy = H * 0.5f;
        float s = std::min(W, H) * 0.38f;

        auto isInsidePenrose = [&](float px, float py) -> bool {
            float x = (px - cx) / s;
            float y = (py - cy) / s;
            float room_half_width = 1.5f;
            bool  top = (y < -0.4f) && ((x * x) / (room_half_width * room_half_width) + pow((y + 0.4f) / 0.7f, 2) <= 1.0f);
            bool  bottom = (y > 0.4f) && ((x * x) / (room_half_width * room_half_width) + pow((y - 0.4f) / 0.7f, 2) <= 1.0f);
            bool  middle_rect = (abs(y) <= 0.4f) && (abs(x) <= room_half_width);
            bool  room_base = top || bottom || middle_rect;

            float mush_pos_x = 1.3f;
            auto  is_mushroom = [&](float mx, float my) -> bool {
                bool cap = (mx > 0.0f) && (my * my / 0.16f + mx * mx / 0.09f <= 1.0f);
                bool stem = (mx <= 0.0f) && (abs(my) <= 0.08f) && (mx > -0.6f);
                return cap || stem;
                };
            bool left_mushroom = is_mushroom(x - (-mush_pos_x), y);
            bool right_mushroom = is_mushroom(-(x - mush_pos_x), y);
            return room_base && !left_mushroom && !right_mushroom;
            };

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                if (isInsidePenrose((float)x, (float)y)) setMaterialAt(x, y, rockVp, rockVs, rockRho);
                else                                     setMaterialAt(x, y, airVp, airVs, airRho);
            }
        }
        edit_src_x = static_cast<int>(cx);
        edit_src_z = static_cast<int>(cy);
    }
    else if (type == SCENE_TOPOGRAPHY) { // ����Ϊ SCENE_TOPOGRAPHY
        // 1. ���屳������ (����Χ��)
        float rockVp = 2500.0f, rockVs = 1443.0f, rockRho = 2000.0f;
        // 2. ���������Ч���� (��շ�)
        float airVp = 340.0f, airVs = 0.0f, airRho = 50.0f; // �ܶ���Ϊ50�Ա�֤���޲��׿Խ���ȶ���

        float baseHeight = H * 0.45f; // ɽ�׻����߶�
        float hillAmp = H * 0.15f; // ɽ��߶�
        float hillFreq = 2.0f * 3.14159f / W * 1.5f; // ����ɽ��ĸ���

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                // �������Һ������˹������һ�����ɽ�������� z = f(x)
                float surfaceZ = baseHeight + hillAmp * sin(x * hillFreq);

                // ע�⣺�� CPU ����ϵ�У�y ԽС�������Խ�
                // ��� y ����ɽ�ر���߶ȣ�˵���õ㴦�ڵر��Ϸ���������
                if (y > surfaceZ) {
                    setMaterialAt(x, y, airVp, airVs, airRho);   // �����ٿ���
                }
                else {
                    setMaterialAt(x, y, rockVp, rockVs, rockRho); // ������ɽ����ʯ
                }
            }
        }
        // �����㶨λ��ɽ���ڲ�
        edit_src_x = W / 2;
        edit_src_z = H / 2;
        }
}


// =============================================================================
// 12. ���Գ�����װһ��Ӧ������
// =============================================================================
/** Stops the active run, rebuilds the selected model, refreshes coefficients, and reallocates GPU state. */
inline void applyScenario(int type, SimState& state) {
    state.running = false;
    current_it = 0;
    accumulated_compute_time = 0.0f;
    active_sources.clear(); // ��ն�㼤������

    // 1. ���¹��� CPU ���������ο��
    setupTestContext(ctx, par);

    // 2. װ��Ŀ��ز�ģ��
    loadScenario(type);

    // 3. ʵʱ���¼�������������ƥ�������Ӧ PML ����ϵ��
    updatePmlDampingArrays();
    autoAlignTimeStep();
    // 4. �������ɱ��� dp_flat �迹
    ctx.dp_flat.assign(ctx.NX, 0.0f);
    int fs_idx = ctx.npml;
    for (int j = 0; j < ctx.NX; ++j) {
        int k = fs_idx * ctx.NX + j;
        if (k < ctx.total_grid && ctx.lambda2mu[k] > 0.0f) {
            float l2m = ctx.lambda2mu[k];
            float lam = ctx.lambda[k];
            ctx.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
        }
    }

    // 5. ע������װ����ʼ�� GPU �Դ�
    releaseGpuSimulation(gpu_data);
    initializeGpuSimulation(gpu_data, ctx);

    // 6. ͬ���ϴ�������ά��������������ϵͳ��ֵ����
    updateModelTexture();
    recalculateCachedModelMetrics();
}

/// ////////////////////////////////////////////////////////////////////////��������
// =============================================================================
// 13. ϵͳԭ�����ļ�ѡ��Ի��� (Windows/Linux ��ƽ̨˫ģ)
// =============================================================================
inline bool openSystemFileDialog(char* buffer, int bufferSize) {
    memset(buffer, 0, bufferSize);

#ifdef _WIN32
    // Windows ԭ���ײ�ʵ��
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = bufferSize;
    ofn.lpstrFilter = "Model Data (*.txt;*.sgy;*.csv)\0*.txt;*.sgy;*.segy;*.csv\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1; // Ĭ��ѡ�е�һ�� (Model Data)
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn);
#else
    // Linux ԭ��ʵ�� (��Ҫ�ն�Ԥװ zenity �������)
    const char* cmd = "zenity --file-selection --file-filter='*.txt' --title='Select Model Data'";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        std::cerr << "[IO Error] Zenity not found. Please install it to use the file dialog." << std::endl;
        return false;
    }
    bool success = false;
    if (fgets(buffer, bufferSize, pipe) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0'; // �ü�ĩβ���з�
        }
        success = (strlen(buffer) > 0);
    }
    pclose(pipe);
    return success;
#endif
}

// =============================================================================
// 14. ϵͳԭ�������ļ�ѡ��Ի��� (Windows/Linux ��ƽ̨˫ģ)
// =============================================================================
#ifdef _WIN32
inline bool saveSystemFileDialog(char* outPath, DWORD maxPathLen) {
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = outPath;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = maxPathLen;
    ofn.lpstrFilter = "Seismic SEGY (*.sgy)\0*.sgy;*.segy\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    // OFN_NOCHANGEDIR ����ؼ�����ֹ���ĳ�������Ŀ¼�������·����ɫ��ʧЧ
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    return GetSaveFileNameA(&ofn) == TRUE;
}
#else
inline bool saveSystemFileDialog(char* outPath, size_t maxPathLen) {
    return false;
}
#endif

// =============================================================================
// 15. ������ (Column-Major) 2D ����ģ�������ز����㷨 (����ڲ�ֵ)
// =============================================================================
// Resamples a 2D material grid to the requested dimensions using interpolation.
inline std::vector<std::vector<float>> resampleGrid2d(
    const std::vector<std::vector<float>>& input,
    float                                  target_dx,
    float                                  original_dx
) {
    if (input.empty() || input[0].empty()) return {};

    int old_nx = static_cast<int>(input.size());    // ԭʼ X ���� (Traces)
    int old_nz = static_cast<int>(input[0].size()); // ԭʼ Z ���� (Samples)

    // ����ʵ�ʵ��������������
    float physical_width = (old_nx - 1) * original_dx;
    float physical_depth = (old_nz - 1) * original_dx;

    // �����Ӧ target_dx �����µ����������
    int new_nx = static_cast<int>(physical_width / target_dx) + 1;
    int new_nz = static_cast<int>(physical_depth / target_dx) + 1;

    std::cout << "[Resample] Resampling grid: " << old_nx << "x" << old_nz
        << " -> " << new_nx << "x" << new_nz
        << " (spacing: " << original_dx << "m -> " << target_dx << "m)" << std::endl;

    std::vector<std::vector<float>> output(new_nx, std::vector<float>(new_nz, 0.0f));
    float ratio = target_dx / original_dx;

    for (int x = 0; x < new_nx; ++x) {
        int old_x = static_cast<int>(x * ratio + 0.5f);
        if (old_x >= old_nx) old_x = old_nx - 1;

        for (int z = 0; z < new_nz; ++z) {
            int old_z = static_cast<int>(z * ratio + 0.5f);
            if (old_z >= old_nz) old_z = old_nz - 1;

            output[x][z] = input[old_x][old_z];
        }
    }
    return output;
}

// =============================================================================
// 16. ������ 2D �����ز�߽������������� (�����尲ȫ�̻�����)
// =============================================================================
// Extends a 2D grid with absorbing-boundary padding while preserving its interior.
inline std::vector<std::vector<float>> padPmlToGrid2d(
    const std::vector<std::vector<float>>& grid_vp,
    const std::vector<std::vector<float>>& grid_vs,
    const std::vector<std::vector<float>>& grid_rho,
    int                                    npml,
    int                                    component_type // 0: Vp, 1: Vs, 2: Rho
) {
    if (grid_vp.empty() || grid_vp[0].empty()) return {};

    int old_nx = static_cast<int>(grid_vp.size());
    int old_nz = static_cast<int>(grid_vp[0].size());

    int new_nx = old_nx + 2 * npml;
    int new_nz = old_nz + 2 * npml;

    std::vector<std::vector<float>> output(new_nx, std::vector<float>(new_nz, 0.0f));

    for (int x = 0; x < new_nx; ++x) {
        int  src_x = std::max(0, std::min(x - npml, old_nx - 1));
        bool in_x_pml = (x < npml) || (x >= new_nx - npml);

        for (int z = 0; z < new_nz; ++z) {
            int  src_z = std::max(0, std::min(z - npml, old_nz - 1));
            bool in_z_pml = (z < npml) || (z >= new_nz - npml);

            // ��ȡ���ƶ�����ԭʼ������
            float vp = grid_vp[src_x][src_z];
            float vs = grid_vs[src_x][src_z];
            float rho = grid_rho[src_x][src_z];

            // ��������ת�� (g/cm3 ת kg/m3)
            if (convert_rho) {
                if (rho > 0.0f && rho < 10.0f) rho *= 1000.0f;
                if (rho < 10.0f) rho = 1000.0f;
            }

            // ����õ㴦�� PML �ڲ�����ԭ����Һ̬�ز㣨Vs == 0��
            // �� PML �ڲ�ǿ�н��䡰�̻���Ϊ���в���Ϊ Vp / 2 ���ȶ�������ʣ����� PML ���
            if (in_x_pml || in_z_pml) {
                if (vs <= 0.0f && vp > 350.0f) {
                    vs = vp / 2.0f; // �ȶ������ٶ�
                }
            }

            // ��������ķ�����������ֵ
            if (component_type == 0)      output[x][z] = vp;
            else if (component_type == 1) output[x][z] = vs;
            else                          output[x][z] = rho;
        }
    }
    return output;
}

// =============================================================================
// 17. ������ ASCII ��ʽ (TXT) ����ģ�͵�����
// =============================================================================
// Loads a text material model, validates its dimensions, and refreshes model views.
inline bool loadModelFromText(const std::string& filename, bool flipVertically, GLHandles& gl, SimState& state) {
    std::cout << "[IO] Loading custom ASCII model: " << filename << " (Flip Y: " << (flipVertically ? "Yes" : "No") << ")" << std::endl;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[IO] Error: Could not open model file: " << filename << std::endl;
        return false;
    }

    // 1. ���� Dimensions ͷ���ߴ�
    std::string line;
    int         file_w = 0, file_h = 0;
    bool        dimensionsFound = false;

    while (std::getline(file, line)) {
        if (line.rfind("# Dimensions", 0) == 0) {
            sscanf_s(line.c_str(), "# Dimensions (Width x Height): %d %d", &file_w, &file_h);
            if (file_w > 0 && file_h > 0) dimensionsFound = true;
            break;
        }
    }

    if (!dimensionsFound) {
        std::cerr << "[IO] Error: Missing '# Dimensions' header." << std::endl;
        return false;
    }

    // ���� CPU ��ʱ 2D ԭʼ��������
    std::vector<std::vector<float>> raw_vp(file_w, std::vector<float>(file_h, 0.0f));
    std::vector<std::vector<float>> raw_vs(file_w, std::vector<float>(file_h, 0.0f));
    std::vector<std::vector<float>> raw_rho(file_w, std::vector<float>(file_h, 0.0f));

    file.clear();
    file.seekg(0, std::ios::beg);

    // 2. ���н�����װ����ʱ����
    int pixelCount = 0;
    int total_pixels = file_w * file_h;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        float vp = 0.0f, vs = 0.0f, rho = 0.0f;
        if (!(ss >> vp >> vs >> rho)) continue;

        if (pixelCount >= total_pixels) break;

        int currentX = pixelCount % file_w;
        int currentY = pixelCount / file_w;

        raw_vp[currentX][currentY] = vp;
        raw_vs[currentX][currentY] = vs;
        raw_rho[currentX][currentY] = rho;

        pixelCount++;
    }
    file.close();

    if (pixelCount < total_pixels) {
        std::cerr << "[IO] Error: Custom model data is incomplete." << std::endl;
        return false;
    }

    // ��̬�ز��� (����ϵͳ��ǰ�������ռ䲽�� par.model.dx)
    float original_txt_dx = 1.0f;
    float target_system_dx = par.model.dx;

    auto grid_vp = resampleGrid2d(raw_vp, target_system_dx, original_txt_dx);
    auto grid_vs = resampleGrid2d(raw_vs, target_system_dx, original_txt_dx);
    auto grid_rho = resampleGrid2d(raw_rho, target_system_dx, original_txt_dx);

    // PML ���������尲ȫ�̻� (һ���ǳ�)
    int  current_npml = static_cast<int>(par.FDM.npml);
    auto vp_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 0); // �� 0 �� Vp
    auto vs_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 1); // �� 1 �� Vs
    auto rho_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 2); // �� 2 �� Rho

    // 3. ����ģ���ݻ������������������
    state.running = false;
    current_it = 0;
    accumulated_compute_time = 0.0f;
    active_sources.clear();

    edit_w = static_cast<int>(vp_pad.size());
    edit_h = static_cast<int>(vp_pad[0].size());
    par.model.xnum = edit_w;
    par.model.znum = edit_h;

    // ������ CPU �����������������С
    setupTestContext(ctx, par);

    // 4. ��������������������ע�� CPU �ڴ�
    for (int x = 0; x < ctx.NX; ++x) {
        for (int z = 0; z < ctx.NZ; ++z) {
            float vp = vp_pad[x][z];
            float vs = vs_pad[x][z];
            float rho = rho_pad[x][z];
            int   tgtX = x;
            int   tgtZ = z;

            if (flipVertically) {
                tgtZ = ctx.NZ - 1 - z; // ��ֱ�ر�����
            }
            setMaterialAt(tgtX, tgtZ, vp, vs, rho);
        }
    }

    // 5. ������Դλ������沨 dp_flat
    ctx.src_z_idx = ctx.NZ / 4;
    ctx.src_idx = ctx.src_z_idx * ctx.NX + (ctx.NX / 2);
    edit_src_x = ctx.NX / 2;
    edit_src_z = ctx.NZ / 4;

    // ���¼��㲢�궨��߲����µ� PML ����ϵ��
    updatePmlDampingArrays();
    autoAlignTimeStep();

    ctx.dp_flat.assign(ctx.NX, 0.0f);
    int fs_idx = ctx.npml;
    for (int j = 0; j < ctx.NX; ++j) {
        int k = fs_idx * ctx.NX + j;
        if (k < ctx.total_grid && ctx.lambda2mu[k] > 0.0f) {
            float l2m = ctx.lambda2mu[k];
            float lam = ctx.lambda[k];
            ctx.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
        }
    }

    // 6. �ͷŲ�����ע�� FBO �����ߴ磬��ֹ�Դ����
    releaseGpuSimulation(gpu_data);
    cudaGraphicsUnregisterResource(gl.cudaSeisRes);
    glBindTexture(GL_TEXTURE_2D, gl.seisTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ctx.NX, ctx.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

    // 7. ���³�ʼ�����Դ�
    initializeGpuSimulation(gpu_data, ctx);

    // 8. ͬ�����µ��ʱ������������±궨��ֵ���ӿڶ���
    updateModelTexture();
    recalculateCachedModelMetrics();
    first_align_needed = true;

    std::cout << "[IO] Custom ASCII Model Loaded, Padded and Stabilized Successfully: " << ctx.NX << "x" << ctx.NZ << std::endl;
    return true;
}

// =============================================================================
// 18. ������ SEG-Y �������ģ�ͼ����� (֧��˫��ת�������ز���)
// =============================================================================
// Loads velocity/material samples from SEG-Y and updates the selected model state.
inline bool loadModelFromSegy(
    const std::string& vp_path,
    const std::string& vs_path,
    const std::string& rho_path,
    bool               flipVertically,
    bool               flipHorizontally,
    GLHandles& gl,
    SimState& state
) {
    std::cout << "[IO] Loading SEGY model files..." << std::endl;

    auto raw_vp = SeismicIO::readSegyFile2D(vp_path);
    auto raw_vs = SeismicIO::readSegyFile2D(vs_path);
    auto raw_rho = SeismicIO::readSegyFile2D(rho_path);

    if (raw_vp.empty() || raw_vs.empty() || raw_rho.empty()) {
        std::cerr << "[IO] Error: One or more SEGY files failed to load." << std::endl;
        return false;
    }

    // 1. ��̬�ز��� (�������ǵ�ǰϵͳ�������ռ䲽�� ctx.h / par.model.dx)
    float original_segy_dx = edit_segy_dx;
    float target_system_dx = par.model.dx;

    auto grid_vp = resampleGrid2d(raw_vp, target_system_dx, original_segy_dx);
    auto grid_vs = resampleGrid2d(raw_vs, target_system_dx, original_segy_dx);
    auto grid_rho = resampleGrid2d(raw_rho, target_system_dx, original_segy_dx);

    // 2. PML ���� + 100% P���迹ƥ�� + ˮ�㰲ȫ�̻�
    int  current_npml = static_cast<int>(par.FDM.npml);
    auto vp_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 0); // �� 0 �� Vp
    auto vs_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 1); // �� 1 �� Vs
    auto rho_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 2); // �� 2 �� Rho

    // ȷ�����ճߴ�
    int file_w = static_cast<int>(vp_pad.size());
    int file_h = static_cast<int>(vp_pad[0].size());

    state.running = false;
    current_it = 0;
    accumulated_compute_time = 0.0f;
    active_sources.clear();

    edit_w = file_w;
    edit_h = file_h;
    par.model.xnum = edit_w;
    par.model.znum = edit_h;

    // ������ CPU ���ʼ�������ĳߴ�
    setupTestContext(ctx, par);

    // 3. ���������롢��ȫ�ȶ�������д���ڴ� (֧�ִ�ֱ/ˮƽ��ת)
    for (int x = 0; x < ctx.NX; ++x) {
        for (int z = 0; z < ctx.NZ; ++z) {
            float vp = vp_pad[x][z];
            float vs = vs_pad[x][z];
            float rho = rho_pad[x][z];

            int tgtX = x;
            if (flipHorizontally) tgtX = ctx.NX - 1 - x;

            int tgtZ = z;
            if (flipVertically)   tgtZ = ctx.NZ - 1 - z;

            setMaterialAt(tgtX, tgtZ, vp, vs, rho);
        }
    }

    // 4. ���ü���λ���� PML ���沨 dp_flat
    ctx.src_z_idx = ctx.NZ / 4;
    ctx.src_idx = ctx.src_z_idx * ctx.NX + (ctx.NX / 2);
    edit_src_x = ctx.NX / 2;
    edit_src_z = ctx.NZ / 4;

    ctx.dp_flat.assign(ctx.NX, 0.0f);
    int fs_idx = ctx.npml;
    for (int j = 0; j < ctx.NX; ++j) {
        int k = fs_idx * ctx.NX + j;
        if (k < ctx.total_grid && ctx.lambda2mu[k] > 0.0f) {
            float l2m = ctx.lambda2mu[k];
            float lam = ctx.lambda[k];
            ctx.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
        }
    }
    updatePmlDampingArrays();
    autoAlignTimeStep();
    // 5. �ͷŲ�����ע�� FBO �����ߴ磬��ֹ�Դ����
    releaseGpuSimulation(gpu_data);
    cudaGraphicsUnregisterResource(gl.cudaSeisRes);
    glBindTexture(GL_TEXTURE_2D, gl.seisTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ctx.NX, ctx.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

    // 6. ���³�ʼ���´�С�� GPU �Դ�
    initializeGpuSimulation(gpu_data, ctx);

    // 7. ͬ���ػ���ʱ������������¶����ӿڱ��
    updateModelTexture();
    recalculateCachedModelMetrics();
    first_align_needed = true;

    return true;
}

// =============================================================================
// 19. �ռ���ز�ģ�͵����� (֧�ֿռ�ֲ��ü� Cropping ��ˮƽ��ת����Ϊ SGY ��¼)
// =============================================================================
// =============================================================================
// ���ع��󡿣�֧�ֿռ�ֲ��ü���˫��ת�Լ���̬�ز����� SEGY ģ�͵�������
// =============================================================================
// Writes the current material model to SEG-Y with the selected axis metadata.
inline bool exportModelToSegy(
    const std::string& base_filepath,
    bool               density_gcm3,
    bool               flipHorizontally,
    int                x_min,
    int                z_min,
    int                x_max,
    int                z_max,
    float              target_dx // <-- ��������������Ŀ�굼�����񲽳� (��)
) {
    if (ctx.total_grid <= 0) return false;

    // 1. ��ȫ�߽���������Ӧ����
    x_min = std::clamp(x_min, 0, ctx.NX - 1);
    x_max = std::clamp(x_max, 0, ctx.NX - 1);
    z_min = std::clamp(z_min, 0, ctx.NZ - 1);
    z_max = std::clamp(z_max, 0, ctx.NZ - 1);

    if (x_min > x_max) std::swap(x_min, x_max);
    if (z_min > z_max) std::swap(z_min, z_max);

    // ����ü������ģ�ͳߴ� (�¿��ȡ������)
    int new_NX = x_max - x_min + 1;
    int new_NZ = z_max - z_min + 1;

    std::cout << "[IO] Preparing SEGY Export with Cropping..." << std::endl;
    std::cout << "     - Crop Range: X[" << x_min << " ~ " << x_max << "], Z[" << z_min << " ~ " << z_max << "]" << std::endl;
    std::cout << "     - Raw Crop Size: " << new_NX << "x" << new_NZ << std::endl;

    // 2. ��ȡ�ü����� CPU 2D ��ʱ������
    std::vector<std::vector<float>> cropped_vp(new_NX, std::vector<float>(new_NZ, 0.0f));
    std::vector<std::vector<float>> cropped_vs(new_NX, std::vector<float>(new_NZ, 0.0f));
    std::vector<std::vector<float>> cropped_rho(new_NX, std::vector<float>(new_NZ, 0.0f));

    for (int x = 0; x < new_NX; ++x) {
        // ���������ˮƽ��ת����Բü�����������������Ҿ���
        int targetX = flipHorizontally ? (x_max - x) : (x_min + x);

        for (int z = 0; z < new_NZ; ++z) {
            int targetZ = z_min + z; // �������

            int k_cuda = targetZ * ctx.NX + targetX; // ��ȡȫ�����ж�Ӧ��λ��

            float rho_val = ctx.rho[k_cuda];
            float vp_val = 0.0f;
            float vs_val = 0.0f;

            if (rho_val > 0.0f) {
                vp_val = sqrtf(ctx.lambda2mu[k_cuda] / rho_val);
                vs_val = sqrtf(ctx.mu[k_cuda] / rho_val);
            }

            cropped_vp[x][z] = vp_val;
            cropped_vs[x][z] = vs_val;

            // ������λ����
            if (density_gcm3) cropped_rho[x][z] = rho_val / 1000.0f;
            else              cropped_rho[x][z] = rho_val;
        }
    }

    // 3. �������������������ز������߶���
    std::vector<std::vector<float>> final_vp, final_vs, final_rho;
    int export_NX = new_NX;
    int export_NZ = new_NZ;

    // ֻ�е�Ŀ�경������ 0 ���뵱ǰ������ͬʱ���Ŵ�����ά�ز�����ֵ
    if (target_dx > 0.0f && std::abs(target_dx - ctx.h) > 1e-4f) {
        std::cout << "     - [Trigger] Resampling sub-grid: " << ctx.h << "m -> " << target_dx << "m" << std::endl;
        final_vp = resampleGrid2d(cropped_vp, target_dx, ctx.h);
        final_vs = resampleGrid2d(cropped_vs, target_dx, ctx.h);
        final_rho = resampleGrid2d(cropped_rho, target_dx, ctx.h);

        // ˢ���ز��������ʵ�ߴ�
        export_NX = static_cast<int>(final_vp.size());
        export_NZ = static_cast<int>(final_vp[0].size());
    }
    else {
        final_vp = cropped_vp;
        final_vs = cropped_vs;
        final_rho = cropped_rho;
    }

    // 4. �� 2D ��������չƽΪ 1D ��������
    size_t             export_total_grid = static_cast<size_t>(export_NX) * export_NZ;
    std::vector<float> flat_vp(export_total_grid);
    std::vector<float> flat_vs(export_total_grid);
    std::vector<float> flat_rho(export_total_grid);

    for (int x = 0; x < export_NX; ++x) {
        for (int z = 0; z < export_NZ; ++z) {
            size_t k_segy = (size_t)x * export_NZ + z; // SEGY Ҫ��ĵ������д���
            flat_vp[k_segy] = final_vp[x][z];
            flat_vs[k_segy] = final_vs[x][z];
            flat_rho[k_segy] = final_rho[x][z];
        }
    }

    // 5. �Զ���ʽ���ļ�·����д���������ļ�
    std::string clean_path = base_filepath;
    size_t      dot_pos = clean_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        clean_path = clean_path.substr(0, dot_pos);
    }
    std::string out_vp_path = clean_path + "_vp.sgy";
    std::string out_vs_path = clean_path + "_vs.sgy";
    std::string out_rho_path = clean_path + "_rho.sgy";

    float dummy_dt = 0.001f; // ģ���ļ��ı�ƴ�ֱ���
    SeismicIO::writeSegyFile2D(flat_vp, export_NX, export_NZ, out_vp_path, dummy_dt);
    SeismicIO::writeSegyFile2D(flat_vs, export_NX, export_NZ, out_vs_path, dummy_dt);
    SeismicIO::writeSegyFile2D(flat_rho, export_NX, export_NZ, out_rho_path, dummy_dt);

    std::cout << "[IO] Resampled SEGY Model Group Export Completed successfully." << std::endl;
    std::cout << "     - Final Output Size: " << export_NX << " traces x " << export_NZ << " samples" << std::endl;
    return true;
}

// =============================================================================
// 20. ��׼������ SEG-Y ���𲨼�¼�ļ����ݼ�����
// =============================================================================
// Loads recorded seismic traces from SEG-Y into the analysis state.
inline bool loadSeismicSegy(const std::string& filepath, AnalysisState& state) {
    // 1. ���õײ�ı�׼ SEGY 2D �ӿڶ�ȡ
    state.traces = SeismicIO::readSegyFile2D(filepath);
    if (state.traces.empty()) {
        return false;
    }

    state.numChannels = static_cast<int>(state.traces.size());
    state.numSamples = static_cast<int>(state.traces[0].size());

    // 2. �Զ��Ӷ����ƾ�ͷ�� Bytes 17-18 (���ļ��� 3216 �ֽ�) ��ȡ�������
    state.samplingInterval = 0.001f; // Ĭ�Ϸ���ֵ (1ms)
    std::ifstream file(filepath, std::ios::binary);

    if (file.is_open()) {
        file.seekg(3200 + 16, std::ios::beg); // ��λ�� 3216 �ֽ�
        uint16_t raw_dt = 0;
        file.read(reinterpret_cast<char*>(&raw_dt), 2);

        // �����תС����
        uint16_t swapped_dt = ((raw_dt & 0xFF00) >> 8) | ((raw_dt & 0x00FF) << 8);
        if (swapped_dt > 0) {
            state.samplingInterval = static_cast<float>(swapped_dt) / 1000000.0f; // ΢�뻻��Ϊ��
        }
        file.close();
    }

    // 3. ����ȫ������������ Wiggle/Heatmap ��̬�߽�����Ӧ
    state.globalMaxAmp = 0.0f;
    for (const auto& trace : state.traces) {
        for (float val : trace) {
            float abs_val = std::abs(val);
            if (abs_val > state.globalMaxAmp) {
                state.globalMaxAmp = abs_val;
            }
        }
    }
    return true;
}

// =============================================================================
// ����������������ѡ��Ķ������ļ����У��Զ���������� NX �� NZ ��С (��ʽ��_NXxNZ_)
//  ֧�ָ�ʽ�磺marmousi_2267x1401_vz.bin -> w = 2267, z = 1401
// =============================================================================
// Extracts optional grid dimensions encoded in a model filename.
inline bool parseDimensionsFromPath(const std::string& path, int& out_w, int& out_z) {
    // 1. Ѱ�����һ����ĸ 'x' �� 'X'
    size_t last_x = path.find_last_of("xX");
    if (last_x == std::string::npos) return false;

    // 2. �� 'x' ����Ѱ��ǰһ���»��� '_' ��б��
    size_t start_idx = path.find_last_of('_', last_x);
    if (start_idx == std::string::npos) {
        start_idx = path.find_last_of("\\/", last_x);
        if (start_idx == std::string::npos) start_idx = 0;
        else                                start_idx += 1;
    }
    else {
        start_idx += 1; // �����»����ַ�
    }

    // 3. �� 'x' ����Ѱ�Һ�һ���»��� '_' ���� '.'
    size_t end_idx = path.find_first_of('_', last_x);
    if (end_idx == std::string::npos) {
        end_idx = path.find_last_of('.');
        if (end_idx == std::string::npos || end_idx < last_x) {
            end_idx = path.size();
        }
    }

    if (start_idx >= last_x || last_x >= end_idx) return false;

    // 4. ��ȡ���Ⱥ�������ַ�����תΪ����
    std::string w_str = path.substr(start_idx, last_x - start_idx);
    std::string z_str = path.substr(last_x + 1, end_idx - last_x - 1);

    // ������֤��ȷ����ȡ���ַ���ȫ��Ϊ������
    auto is_digits = [](const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
        };

    if (is_digits(w_str) && is_digits(z_str)) {
        out_w = std::stoi(w_str);
        out_z = std::stoi(z_str);
        return true;
    }
    return false;
}

// =============================================================================
// �첨����������ͬ���� GPU �Դ�ĺ��Ľӿ�
// =============================================================================
// Synchronizes receiver coordinates with the GPU sampling buffers.
inline void syncReceiversToGpu() {
    std::vector<int> receiverIndices;
    receiverIndices.reserve(rec.receivers.size());
    for (const ReceiverPos& receiver : rec.receivers) {
        if (receiver.x < 0 || receiver.x >= ctx.NX || receiver.y < 0 || receiver.y >= ctx.NZ) {
            throw std::invalid_argument("Receiver coordinates are outside the model; redeploy the receiver array");
        }
        receiverIndices.push_back(receiver.y * ctx.NX + receiver.x);
    }
    updateGpuReceivers(gpu_data, receiverIndices);
    ctx.rcv_grid_idx = std::move(receiverIndices);
    ctx.num_rcv = static_cast<int>(ctx.rcv_grid_idx.size());
    rec.numChannels = ctx.num_rcv;
}

// =============================================================================
// ���ݲɼ�һ�������뵥�㼤����������
// =============================================================================
// =============================================================================
// ���ݲɼ�һ�������뵥�㼤���������� (�����棺ִ�� GPU ��ʼ��ǰǿ��������������)
// =============================================================================
// Initializes and advances one single-shot simulation using the current setup.
inline void triggerSingleShot(GLHandles& gl, SimState& state) {
    // =========================================================================
    // �������޸������ڳ�ʼ�� GPU ֮ǰ�����뽫��ǰǰ̨�����Զ��������У����µ� 
    //  ��Դ���ꡢ�������͡���ֵ���Ƕ� 100% ͬ����ȫ�� ctx�����ػ��Ӳ������׸���ԭ�ر�ը Bug��
    // =========================================================================
    ctx.src_z_idx = edit_src_z;
    ctx.src_idx = edit_src_z * ctx.NX + edit_src_x;
    ctx.src_angle = edit_angle;
    ctx.src_type = edit_source_type;
    ctx.src_amp = edit_amp;

    // ���¼��㲢����������ʱ�䲽 dt ����� Ricker �Ӳ������׶ž�Ƶ�ʻ���
    generateRickerWavelet(ctx.wavelet, ctx.nt, ctx.dt, edit_f0, edit_t0);

    current_it = 0;
    state.running = true;
    accumulated_compute_time = 0.0f;
    active_sources.clear();

    releaseGpuSimulation(gpu_data);
    initializeGpuSimulation(gpu_data, ctx); // ��ʱ gpu_data �������ȫ���������Դλ���������

    // ǿ��ִ��һ�μ첨���Դ�ͬ��
    syncReceiversToGpu();

    std::fill(ctx.record_vx.begin(), ctx.record_vx.end(), 0.0f);
    std::fill(ctx.record_vz.begin(), ctx.record_vz.end(), 0.0f);
}

// =============================================================================
// ���ɼ������ݵ���Ϊ����������ͷ����ı�׼ SEG-Y ��ʽ�ļ�
// =============================================================================
// =============================================================================
// ������������桿��һ���Զ����ɼ���¼����Ϊ ˮƽ(Vx) �� ��ֱ(Vz) ˫ͨ�� SEG-Y ��¼��
// =============================================================================
// Exports acquired receiver traces to SEG-Y at the requested path.
inline void exportToSegy(const AcquisitionManager& recorder, const std::string& filename) {
    if (recorder.recorded_vz.empty() || recorder.recorded_vz[0].empty()) return;
    const float sample_interval_dt = recorder.actualSampleInterval;
    if (!std::isfinite(sample_interval_dt) || sample_interval_dt <= 0.0f || recorder.numChannels <= 0 ||
        recorder.receivers.size() != recorder.numChannels || recorder.recorded_vx.size() != recorder.numChannels ||
        recorder.recorded_vz.size() != recorder.numChannels) {
        throw std::invalid_argument("Export requires matching traces, receivers, and recorded sampling interval");
    }

    // 1. ƴװ��׼�ĵ����ɼ�Ԫ���ݵ�ͷ
    std::vector<SeismicIO::TraceMetadata> metadata(recorder.numChannels);
    for (int i = 0; i < recorder.numChannels; ++i) {
        metadata[i].fieldRecordNum = recorder.currentFileIndex;
        metadata[i].traceInRecord = i + 1;
        metadata[i].sourceX = static_cast<int32_t>(edit_src_x * ctx.h);
        metadata[i].sourceY = static_cast<int32_t>(edit_src_z * ctx.h);
        metadata[i].groupX = static_cast<int32_t>(recorder.receivers[i].x * ctx.h);
        metadata[i].groupY = static_cast<int32_t>(recorder.receivers[i].y * ctx.h);
    }

    // 2. �Զ���ʽ����ֵ���·��Ϊ _vx �� _vz ��ʽ
    std::string clean_path = filename;
    size_t      dot_pos = clean_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        clean_path = clean_path.substr(0, dot_pos);
    }

    // 3. ��������������һʽ���ݣ�ͬ�������߱���ˮƽ�봹ֱ�����¼������
    SeismicIO::writeSegyFile2D(recorder.recorded_vx, metadata, clean_path + "_vx.sgy", sample_interval_dt); // ����ˮƽ����
    SeismicIO::writeSegyFile2D(recorder.recorded_vz, metadata, clean_path + "_vz.sgy", sample_interval_dt); // ������ֱ����

    std::cout << "[Acquisition IO] Dual-Component (2C) Seismograms exported successfully." << std::endl;
}

// CSV �򵥸�ʽ���ݵ���
// Exports acquired receiver traces to a tabular CSV file.
inline void exportToCsv(const AcquisitionManager& recorder, const std::string& filename) {
    if (recorder.recorded_vz.empty()) return;
    std::ofstream file(filename);
    if (!file.is_open()) return;

    int samples = recorder.recorded_vz[0].size();
    for (int j = 0; j < samples; ++j) {
        for (int i = 0; i < recorder.numChannels; ++i) {
            file << recorder.recorded_vz[i][j];
            if (i < recorder.numChannels - 1) file << ",";
        }
        file << "\n";
    }
    file.close();
}

/// ////////////////////////////////////////////////////////////////////////���Ĳ���

// =============================================================================
// 21. ���Ը���һά Heatmap ����ĸ�������
// =============================================================================
// Recomputes the analysis heatmap from the currently selected trace data.
inline void updateHeatmapData(AnalysisState& state) {
    state.heatmapData.resize(state.numSamples * state.numChannels);
    for (int r = 0; r < state.numSamples; ++r) {
        for (int c = 0; c < state.numChannels; ++c) {
            // �� [channel_idx][sample_idx] ӳ��Ϊ�����Ⱦ����������� ImPlot::PlotHeatmap
            state.heatmapData[r * state.numChannels + c] = state.traces[c][r];
        }
    }
}

// =============================================================================
// 22. �ӿ� LOD ��Ч�ص���ȡ��
// =============================================================================
// Supplies one receiver-trace sample to ImPlot using the current plot data.
inline ImPlotPoint seismicDataGetter(int idx, void* data) {
    PlotContext* ctx = reinterpret_cast<PlotContext*>(data);
    int          sampleIdx = ctx->startSample + idx * ctx->step;
    float        val = (*ctx->traceData)[sampleIdx];

    // �������չ��ƫ����ʱ�����ת��
    double x = ctx->offsetX + val * ctx->gain;
    double y = ctx->useTime ? (sampleIdx * ctx->dt) : (double)sampleIdx;

    return ImPlotPoint(x, y);
}

// ȫ��Ψһ�ķ�����״̬ʵ��
inline AnalysisState g_analyzerState;

// =============================================================================
// 23. �߱��� SEG-Y ̽�����ݷ����������� (Seismic Data Analyzer Window)
// =============================================================================
void renderAnalysisWindow(AnalysisState& state) {
    if (!state.isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Seismic Data Analyzer", &state.isOpen)) {
        static bool useTimeAxis = true;

        // ------------------------------------------------------------
        // A. ���������� (SEG-Y ��������������)
        // ------------------------------------------------------------
        if (ImGui::Button("Load SEG-Y Data")) {
            char filePath[1024] = { 0 };
            if (openSystemFileDialog(filePath, sizeof(filePath))) {
                // һ�������׼�Ķ����� SEG-Y ������
                if (loadSeismicSegy(filePath, state)) {
                    // �Զ����� Wiggle �����뷶Χ
                    if (state.globalMaxAmp > 1e-20f) {
                        state.displayGain = 0.8f / state.globalMaxAmp;
                        state.heatmapMin = -state.globalMaxAmp;
                        state.heatmapMax = state.globalMaxAmp;
                    }
                    else {
                        state.displayGain = 1.0f;
                    }
                    state.heatmapData.clear(); // ��������ݣ���������ƴװ
                    state.fitRequest = true;   // һ������Ӧ����ȫ��
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset View")) {
            state.fitRequest = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset axes to full data range.");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("| %d Traces x %d Samples | MaxAmp: %.2e | dt: %.5f s",
            state.numChannels, state.numSamples, state.globalMaxAmp, state.samplingInterval);

        // ------------------------------------------------------------
        // B. �����������
        // ------------------------------------------------------------
        if (ImGui::CollapsingHeader("Display Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "viz_cols", false);

            // --- ���У�Wiggle ����ϸ�߻������� ---
            ImGui::TextDisabled("[ Wiggle Trace ]");
            ImGui::Checkbox("Show Wiggle", &state.showWiggle);
            if (state.showWiggle) {
                ImGui::SameLine();
                ImGui::Checkbox("Time Axis (Y)", &useTimeAxis);

                float baseGain = (state.globalMaxAmp > 0) ? (1.0f / state.globalMaxAmp) : 1.0f;
                float minGain = baseGain * 0.01f;
                float maxGain = baseGain * 1000.0f;

                ImGui::SetNextItemWidth(150);
                if (ImGui::DragFloat("Gain", &state.displayGain, baseGain * 0.01f, minGain, maxGain, "%.2e", ImGuiSliderFlags_Logarithmic)) {
                    if (state.displayGain < minGain) state.displayGain = minGain;
                }

                ImGui::SameLine();
                ImGui::ColorEdit4("Color", (float*)&state.colorLine, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            }

            // --- ���У�Heatmap �ܶ���ɫ�������� ---
            ImGui::NextColumn();
            ImGui::TextDisabled("[ Heatmap Background ]");
            ImGui::Checkbox("Show Heatmap", &state.showHeatmap);
            if (state.showHeatmap) {
                ImGui::SameLine();
                const char* cmaps[] = { "RdBu", "PiYG", "Spectral", "Greys" };
                ImGui::SetNextItemWidth(100);
                ImGui::Combo("##cmap", &state.colormapIndex, cmaps, IM_ARRAYSIZE(cmaps));

                ImGui::SetNextItemWidth(180);
                float dragSpeed = (state.globalMaxAmp > 0) ? state.globalMaxAmp * 0.01f : 0.01f;
                ImGui::DragFloatRange2("Range", &state.heatmapMin, &state.heatmapMax, dragSpeed, -FLT_MAX, FLT_MAX, "Min: %.2e", "Max: %.2e");
            }
            ImGui::Columns(1);

            // --- ֻ��������������ʾ ---
            ImGui::Separator();
            ImGui::Text("Physical Parameters:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "Sampling Interval (dt): %.5f s  (Auto-Parsed from SEGY Header)", state.samplingInterval);
        }

        ImGui::Separator();

        // ------------------------------------------------------------
        // C. ���Ļ�ͼչ���� (���� ImPlot)
        // ------------------------------------------------------------
        if (state.numChannels > 0 && state.numSamples > 0) {
            if (state.showHeatmap && state.heatmapData.empty()) {
                updateHeatmapData(state);
            }

            // ����ѧ���׵׻�ͼֽ���ɫ��
            ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImPlot::PushStyleColor(ImPlotCol_AxisGrid, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImPlot::PushStyleColor(ImPlotCol_AxisText, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImPlot::PushStyleColor(ImPlotCol_TitleText, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

            if (ImPlot::BeginPlot("Seismic Record", ImVec2(-1, -1))) {
                double maxTime = state.numSamples * state.samplingInterval;
                double yEnd = useTimeAxis ? maxTime : (double)state.numSamples;

                const char* yLabel = useTimeAxis ? "Time (s)" : "Sample Index";
                ImPlot::SetupAxes("Trace Number", yLabel);
                ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert); // ��ˣ�ǳ���ڵر��Ӷ�����

                if (state.fitRequest) {
                    ImPlot::SetupAxisLimits(ImAxis_X1, -2.0, state.numChannels + 2.0, ImGuiCond_Always);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -yEnd * 0.05, yEnd * 1.05, ImGuiCond_Always);
                    state.fitRequest = false;
                }
                else {
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -10.0, state.numChannels + 10.0);
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, -yEnd * 0.5, yEnd * 2.0);
                }

                // --- Layer 1: Heatmap �ܶ����汳�� ---
                if (state.showHeatmap && !state.heatmapData.empty()) {
                    ImPlotColormap mapID = ImPlotColormap_RdBu;
                    if (state.colormapIndex == 1)      mapID = ImPlotColormap_PiYG;
                    else if (state.colormapIndex == 2) mapID = ImPlotColormap_Spectral;
                    else if (state.colormapIndex == 3) mapID = ImPlotColormap_Greys;

                    ImPlot::PushColormap(mapID);
                    ImPlot::PlotHeatmap("##HM",
                        state.heatmapData.data(),
                        state.numSamples,
                        state.numChannels,
                        state.heatmapMin, state.heatmapMax,
                        NULL,
                        { 0.5, yEnd },
                        { (double)state.numChannels + 0.5, 0.0 }
                    );
                    ImPlot::PopColormap();
                }

                // --- Layer 2: Wiggle Trace ��������� ---
                if (state.showWiggle) {
                    ImPlotRect limits = ImPlot::GetPlotLimits();
                    ImVec2     plotSize = ImPlot::GetPlotSize();

                    // �ռ�ˮƽ�ӿ��޳���������Ⱦ��Ұ��ĵ�
                    int startCh = (int)floor(limits.X.Min) - 1;
                    int endCh = (int)ceil(limits.X.Max) + 1;
                    if (startCh < 0)                  startCh = 0;
                    if (endCh > state.numChannels) endCh = state.numChannels;

                    // ��������ӿ��޳� (���� Invert ��)
                    double valMin = (limits.Y.Min < limits.Y.Max) ? limits.Y.Min : limits.Y.Max;
                    double valMax = (limits.Y.Min > limits.Y.Max) ? limits.Y.Min : limits.Y.Max;

                    int startSample, endSample;
                    if (useTimeAxis) {
                        startSample = (int)(valMin / state.samplingInterval);
                        endSample = (int)(valMax / state.samplingInterval);
                    }
                    else {
                        startSample = (int)valMin;
                        endSample = (int)valMax;
                    }
                    if (startSample < 0)                 startSample = 0;
                    if (endSample > state.numSamples) endSample = state.numSamples;

                    if (startCh < endCh && startSample < endSample) {
                        // ��̬ LOD �����˲�����ֹ���߷ֱ��ʵ�ͼ�����ع��ർ�¿����ʧЧ/����
                        double visibleSamples = endSample - startSample;
                        int    step = 1;
                        if (visibleSamples > 0 && plotSize.y > 0) {
                            double samplesPerPixel = visibleSamples / plotSize.y;
                            if (samplesPerPixel > 1.0) step = (int)samplesPerPixel;
                        }

                        PlotContext ctx;
                        ctx.gain = state.displayGain;
                        ctx.dt = state.samplingInterval;
                        ctx.useTime = useTimeAxis;
                        ctx.startSample = startSample;
                        ctx.step = step;
                        int pointsToDraw = (endSample - startSample) / step;

                        for (int i = startCh; i < endCh; ++i) {
                            ImPlot::SetNextLineStyle(state.colorLine);
                            ctx.traceData = &state.traces[i];
                            ctx.offsetX = (float)(i + 1);

                            ImGui::PushID(i);
                            if (pointsToDraw > 0) {
                                ImPlot::PlotLineG("##Trace", seismicDataGetter, &ctx, pointsToDraw);
                            }
                            ImGui::PopID(); // <-- �������ָ�Ϊƽ��� PopID()
                        }
                    }
                }
                ImPlot::EndPlot();
            }
            ImPlot::PopStyleColor(5);
        }
        else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(ImGui::GetCursorScreenPos().x + avail.x / 2 - 80, ImGui::GetCursorScreenPos().y + avail.y / 2),
                IM_COL32(128, 128, 128, 255), "No SEG-Y Data Loaded"
            );
        }
    }
    ImGui::End();
}

// =============================================================================
// 24. ��ʼ���Դ�����ֵģ������������Դ
// =============================================================================
/** Creates the initial simulation context, GPU fields, and CUDA-OpenGL wavefield texture once. */
inline void initializeSeismicSimulation(GLHandles& gl) {
    if (!is_seis_initialized) {
        edit_src_x = edit_w / 2;
        edit_src_z = edit_h / 4; // ��ԴĬ�������Ϊ 1/4

        par.model.xnum = edit_w;
        par.model.znum = edit_h;
        par.model.dx = 1.0f;
        par.model.dz = 1.0f;
        par.FDM.f0 = edit_f0;
        par.FDM.t0 = edit_t0;

        setupTestContext(ctx, par);
        initializeGpuSimulation(gpu_data, ctx);

        glGenTextures(1, &gl.seisTex);
        glBindTexture(GL_TEXTURE_2D, gl.seisTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ctx.NX, ctx.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

        is_seis_initialized = true;
        first_align_needed = true;
        updateModelTexture();

        // --- ������������ʼ���Զ����ڵ�Ĭ���������� ---
        g_autoAcq.shotDepthZ = edit_src_z;
        g_autoAcq.startX = edit_w / 5;
    }
}

// =============================================================================
// 25. �����������Ӧƽ������������
// =============================================================================
/** Fits the 2D model aspect ratio to the available viewport after initialization or resize. */
inline void applyCameraAutoAlignment(int winW, int winH, float barHeight) {
    if (first_align_needed && winH > 0 && ctx.NX > 0) {
        float  winAspect = (float)winW / (float)winH;
        float  simAspect = (float)ctx.NX / (float)ctx.NZ;
        ImVec2 aspectCorr = { 1.0f, 1.0f };

        if (winAspect > simAspect) {
            aspectCorr.x = winAspect / simAspect;
        }
        else {
            aspectCorr.y = simAspect / winAspect;
        }

        viewOffset.x = aspectCorr.x - 1.0f;
        viewOffset.y = 1.0f - aspectCorr.y * (1.0f - 2.0f * barHeight / (float)winH);
        first_align_needed = false;
    }
}

// =============================================================================
// 26. �����߼����� (�����/��ק��������������)
// =============================================================================
/** Converts viewport input into model edits, source/receiver placement, and camera controls. */
inline void handleSeismicInteractions(SimState& state, int winW, int winH, float barHeight) {
    ImGuiIO& io = ImGui::GetIO();

    // A. ��Ӧ���� C ��һ�����������ģ���ź�
    if (g_resetSimRequested) {
        current_it = 0;
        state.running = false;
        accumulated_compute_time = 0.0f;
        active_sources.clear();
        ctx.dp_flat.assign(ctx.NX, 0.0f);

        int fs_idx = ctx.npml;
        for (int j = 0; j < ctx.NX; ++j) {
            int k = fs_idx * ctx.NX + j;
            if (k < ctx.total_grid && ctx.lambda2mu[k] > 0.0f) {
                float l2m = ctx.lambda2mu[k];
                float lam = ctx.lambda[k];
                ctx.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
            }
        }

        releaseGpuSimulation(gpu_data);
        initializeGpuSimulation(gpu_data, ctx);
        updateModelTexture();
        std::fill(ctx.record_vx.begin(), ctx.record_vx.end(), 0.0f);
        std::fill(ctx.record_vz.begin(), ctx.record_vz.end(), 0.0f);
        g_resetSimRequested = false;
    }

    // B. ��Ӧ R ����������ӽ�������
    if (g_resetViewportRequested) {
        viewZoom = 1.0f;

        float  winAspect = (float)winW / (float)winH;
        float  simAspect = (float)ctx.NX / (float)ctx.NZ;
        ImVec2 aspectCorr = { 1.0f, 1.0f };

        if (winAspect > simAspect) {
            aspectCorr.x = winAspect / simAspect;
        }
        else {
            aspectCorr.y = simAspect / winAspect;
        }

        // ���¶������Ͻǣ�ȷ���ӽ�������λ
        viewOffset.x = aspectCorr.x - 1.0f;
        viewOffset.y = 1.0f - aspectCorr.y * (1.0f - 2.0f * barHeight / (float)winH);

        g_resetViewportRequested = false;
        active_sources.clear();
    }

    if (!io.WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        // �ܹ��� 7 ���������� (Index 0 ~ 6)
        show_component = (show_component + 1) % 7;
    }
    // =============================================================================
    // F. ������������Ӧ���� W ��һ��ѭ���л� 13 �׿�ѧɫ�ײ�����ʽ
    // =============================================================================
    if (!io.WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        // �ܹ��� 13 �ײ�����ʽ (Index 0 ~ 12)
        waveStyle = (waveStyle + 1) % 13;
    }
    if (!io.WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        // �ܹ��� 5 �ױ������ (Index 0 ~ 4)
        modelStyle = (modelStyle + 1) % 5;
    }

    // ���������������������ּ� 1 - 5 ����һ���л���ǰ��껭�ʹ���ģʽ
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(ImGuiKey_1)) state.brushType = TOOL_NONE;   // 1 �����ָ�Ĭ�ϵ�����Դ
        if (ImGui::IsKeyPressed(ImGuiKey_2)) state.brushType = TOOL_HIGH;   // 2 �����л�ΪӲ�Ҹ�����ˢ
        if (ImGui::IsKeyPressed(ImGuiKey_3)) state.brushType = TOOL_LOW;    // 3 �����л�Ϊ��ɢ�������ˢ
        if (ImGui::IsKeyPressed(ImGuiKey_4)) state.brushType = TOOL_WALL;   // 4 �����л�Ϊ�Զ�������ˢ
        if (ImGui::IsKeyPressed(ImGuiKey_5)) state.brushType = TOOL_ERASER; // 5 �����л�Ϊ��Ƥ��
    }

    // ������Ļ���ű���ϵ��
    float  winAspect = (float)winW / (float)winH;
    float  simAspect = (float)ctx.NX / (float)ctx.NZ;
    ImVec2 aspectCorr = { 1.0f, 1.0f };
    if (winAspect > simAspect) aspectCorr.x = winAspect / simAspect;
    else                         aspectCorr.y = simAspect / winAspect;

    // C. ���������� (����Դ��������Դ˲ʱע��)
    static double last_inject_time = 0.0;
    double        current_time = glfwGetTime();
    bool          trigger_active = false;

    if (multi_source_mode) {
        if (ImGui::IsMouseDown(0) && (current_time - last_inject_time >= 0.02)) {
            trigger_active = true;
            last_inject_time = current_time;
        }
    }
    else {
        if (ImGui::IsMouseClicked(0)) {
            trigger_active = true;
        }
    }

    if (!io.WantCaptureMouse) {
        float normX = io.MousePos.x / (float)winW;
        float normY = io.MousePos.y / (float)winH;

        float worldX = (normX - 0.5f) * (aspectCorr.x / viewZoom) + 0.5f + (0.5f * viewOffset.x);
        float worldY = (normY - 0.5f) * (aspectCorr.y / viewZoom) + 0.5f - (0.5f * viewOffset.y);

        int mx = (int)(worldX * (float)ctx.NX);
        int my = (int)(worldY * (float)ctx.NZ);

        if (state.brushType == TOOL_NONE) {
            // -----------------------------------------------------------------
            // 1. ��Ĭ�ϵ��㼤��ģʽ�������õ�����Դ�����¼���
            // -----------------------------------------------------------------
            if (trigger_active) {
                int min_valid = ctx.npml + 0;
                int max_valid_x = ctx.NX - ctx.npml - 0;
                int max_valid_z = ctx.NZ - ctx.npml - 0;

                if (mx >= min_valid && mx <= max_valid_x && my >= min_valid && my <= max_valid_z) {
                    int clicked_idx = my * ctx.NX + mx;

                    if (multi_source_mode) {
                        GPUSource new_src;
                        new_src.idx = clicked_idx;
                        new_src.t = 0.0f;
                        new_src.f_peak = edit_f0;
                        new_src.amp = 1.0f;

                        if (active_sources.size() < 150) {
                            active_sources.push_back(new_src);
                        }
                        edit_src_x = mx;
                        edit_src_z = my;
                    }
                    else {
                        ctx.src_z_idx = my;
                        ctx.src_idx = clicked_idx;
                        edit_src_x = mx;
                        edit_src_z = my;
                        current_it = 0;
                        state.running = true;
                        accumulated_compute_time = 0.0f;

                        releaseGpuSimulation(gpu_data);
                        initializeGpuSimulation(gpu_data, ctx);
                        std::fill(ctx.record_vx.begin(), ctx.record_vx.end(), 0.0f);
                        std::fill(ctx.record_vz.begin(), ctx.record_vz.end(), 0.0f);
                    }
                }
            }
        }
        else {
            // -----------------------------------------------------------------
            // 2. ��ģ��Ϳ���༭ģʽ������ס������ż����ڵ�ǰλ��ִ������ͿĨ
            // -----------------------------------------------------------------
            if (ImGui::IsMouseDown(0)) {
                int  r_cells = static_cast<int>(state.brushRadius);
                bool model_changed = false;

                // 2.1 �趨��ͬ������̬��Ŀ����������
                float p_vp = 3000.0f, p_vs = 1732.0f, p_rho = 2200.0f;
                if (state.brushType == TOOL_HIGH) {
                    p_vp = 4800.0f; p_vs = 2800.0f; p_rho = 2500.0f; // ������ (Ӳ��)
                }
                else if (state.brushType == TOOL_LOW) {
                    p_vp = 1500.0f; p_vs = 800.0f;  p_rho = 1800.0f; // ������ (��ɢ����/ɰ��)
                }
                else if (state.brushType == TOOL_WALL) {
                    p_vp = edit_Vp; p_vs = edit_Vs;  p_rho = edit_Density; // �Զ������Ա� (�� UI ����)
                }
                else if (state.brushType == TOOL_ERASER) {
                    p_vp = 3000.0f; p_vs = 1732.0f; p_rho = 2200.0f; // ��Ƥ�����ָ�Ϊ������׼��
                }

                // 2.2 �ռ�Բ������Ϳˢ�㷨
                for (int dy = -r_cells; dy <= r_cells; ++dy) {
                    for (int dx = -r_cells; dx <= r_cells; ++dx) {
                        if (dx * dx + dy * dy <= r_cells * r_cells) {
                            int target_x = mx + dx;
                            int target_y = my + dy; // ��Ļ��� (0Ϊ�ر�, NZ-1Ϊ�ײ�)

                            if (target_x >= 0 && target_x < ctx.NX && target_y >= 0 && target_y < ctx.NZ) {
                                // ����������ת������setMaterialAt �ڲ� y=0 ��Ӧ�صף�
                                // ������ڴ˴�����һ�δ�ֱ����ת����y_cpu = NZ - 1 - y_screen
                                int cpu_y = ctx.NZ - 1 - target_y;
                                setMaterialAt(target_x, cpu_y, p_vp, p_vs, p_rho);
                                model_changed = true;
                            }
                        }
                    }
                }

                // 2.3 ���������Դ�ͬ���������ػ�
                if (model_changed) {
                    uploadMaterialToGpu();
                }
            }
        }
        // =============================================================================
        // D. ����м���קƽ��������ӿ����� (�����棺���� aspectCorr ����������קճ�͸�)
        // =============================================================================
        if (!io.WantCaptureMouse) {
            // 1. ��̬���㵱ǰ�ӿڵ�����Ӧ����������ϵ��
            float winAspect = (float)winW / (float)winH;
            float simAspect = (float)ctx.NX / (float)ctx.NZ;
            ImVec2 aspectCorr = { 1.0f, 1.0f };
            if (winAspect > simAspect) {
                aspectCorr.x = winAspect / simAspect;
            }
            else {
                aspectCorr.y = simAspect / winAspect;
            }

            // 2. �����޼�����
            if (io.MouseWheel != 0) {
                float mouseSpeed = 0.1f * viewZoom;
                viewZoom += io.MouseWheel * mouseSpeed;
                if (viewZoom < 0.1f)   viewZoom = 0.1f;
                if (viewZoom > 100.0f) viewZoom = 100.0f;
            }

            // 3. ����м�ƽ�� (�����޸������� aspectCorr ������ʹģ��λ�������λ��ʵ�� 1:1 ��ȷ����)
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                viewOffset.x -= 2.0f * io.MouseDelta.x * aspectCorr.x / (winW * viewZoom);
                viewOffset.y += 2.0f * io.MouseDelta.y * aspectCorr.y / (winH * viewZoom);
            }
        }

        ImVec2 mousePos = ImGui::GetMousePos();

        // E. �����ͣλ������ӳ�������Լ��
        is_mouse_inside = (mousePos.x >= 0.0f && mousePos.x < (float)winW &&
            mousePos.y >= barHeight && mousePos.y < (float)(winH - barHeight));

        if (io.WantCaptureMouse) {
            is_mouse_inside = false;
        }

        hover_valid = false;
        if (is_mouse_inside) {
            float normX = mousePos.x / (float)winW;
            float normY = mousePos.y / (float)winH;

            float worldX = (normX - 0.5f) * (aspectCorr.x / viewZoom) + 0.5f + (0.5f * viewOffset.x);
            float worldY = (normY - 0.5f) * (aspectCorr.y / viewZoom) + 0.5f - (0.5f * viewOffset.y);

            hover_mx = (int)(worldX * (float)ctx.NX);
            hover_my = (int)(worldY * (float)ctx.NZ);

            if (hover_mx >= 0 && hover_mx < ctx.NX && hover_my >= 0 && hover_my < ctx.NZ) {
                int k = hover_my * ctx.NX + hover_mx;
                hover_Rho = ctx.rho[k];
                if (hover_Rho > 0.0f) {
                    hover_Vp = sqrtf(ctx.lambda2mu[k] / hover_Rho);
                    hover_Vs = sqrtf(ctx.mu[k] / hover_Rho);
                    hover_valid = true;
                }
            }
        }
    }
}

/// ////////////////////////////////////////////////////////////////////////���岿��
// =============================================================================
// 27. ִ�� CUDA ���޲��ʱ�ݸ��� (CUDA Finite-Difference Wavefield Update)
// =============================================================================
/** Advances configured CUDA time steps and coordinates recording, replay capture, and run state. */
inline void updateWavefieldSimulation(SimState& state) {
    ImGuiIO& io = ImGui::GetIO();
    bool     can_run = state.running && (infinite_mode || current_it < ctx.nt);

    if (can_run) {
        accumulated_compute_time += io.DeltaTime;

        for (int s = 0; s < steps_per_frame; ++s) {
            if (infinite_mode || current_it < ctx.nt) {
                // A. ��㼤�������������Զ�����
                if (multi_source_mode && !active_sources.empty()) {
                    for (auto it = active_sources.begin(); it != active_sources.end(); ) {
                        it->t += ctx.dt;
                        float t_peak = 1.0f / it->f_peak;
                        if (it->t > 2.5f * t_peak) {
                            it = active_sources.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                    if (!active_sources.empty()) {
                        if (active_sources.size() > kMaximumActiveSources) throw std::invalid_argument("Active source capacity exceeded");
                        CUDA_CHECK(cudaMemcpy(gpu_data.d_active_sources, active_sources.data(), active_sources.size() * sizeof(GPUSource), cudaMemcpyHostToDevice));
                    }
                }

                // B. ִ�� GPU ����˺����ƽ�
                runGpuSimulationStep(gpu_data, current_it, ctx, multi_source_mode ? (int)active_sources.size() : -1);

                // =====================================================================
                // ���������������߱���������ݲɼ������߼�
                // =====================================================================
                if (rec.isRecording) {
                    // ����ʱ�ݲ�����Ŀ��ɼ������ʵĻ�������
                    const int steps_per_sample = rec.samplingStepStride;
                    if (steps_per_sample <= 0 || std::abs(steps_per_sample * ctx.dt - rec.actualSampleInterval) > rec.actualSampleInterval * 1e-5f) {
                        throw std::invalid_argument("Simulation dt changed during acquisition; start a new recording");
                    }

                    if (current_it % steps_per_sample == 0) {
                        int sample_idx = current_it / steps_per_sample;
                        if (rec.numChannels <= 0 || rec.recorded_vx.size() != rec.numChannels || rec.recorded_vz.size() != rec.numChannels) throw std::invalid_argument("Recording buffers do not match receiver channels");
                        int max_samples = static_cast<int>(rec.recorded_vz[0].size());

                        if (sample_idx < max_samples) {
                            // 1. ���� GPU �߲�����ȡ�˺�������ȡ��ǰʱ����Ƭ�����н����������
                            // 1. ���� CPU ��ʱ�������������� GPU ��װ�ӿ���ȡ��ǰʱ�����н������� Vx �� Vz ���
                            static std::vector<float> h_step_vx;
                            h_step_vx.resize(rec.numChannels);
                            static std::vector<float> h_step_vz;
                            h_step_vz.resize(rec.numChannels);
                            recordReceiverStepOnGpu(gpu_data, rec.numChannels, h_step_vx.data(), h_step_vz.data());
                            // 3. ����¼�ƻ�������Ӧ���� Sample ������
                            for (int c = 0; c < rec.numChannels; ++c) {
                                rec.recorded_vx[c][sample_idx] = h_step_vx[c];
                                rec.recorded_vz[c][sample_idx] = h_step_vz[c];
                            }
                        }
                    }

                    // =========================================================
                    // �������޸�����ֻ���ڷ�ȫ�Զ����ڹ�����״̬�£��ż�����Զ�ͣ����
                    //  ����Է�ֹ����ͣ���߼����Զ�����ʱ����;����������ռ���������У�
                    // =========================================================
                    rec.totalTimer = current_it * ctx.dt;
                    if (g_autoAcq.state != AUTO_ACQ_RUNNING) {
                        
                        if (rec.totalTimer >= rec.totalDurationSec) {
                            rec.isRecording = false;
                            state.running = false;

                            exportToSegy(rec, "auto_export_gather.sgy");
                            popup_message = "Simulation recording completed!\nExported to 'auto_export_gather.sgy'";
                            show_success_popup = true;
                        }
                    }
                }

                // =============================================================
                // ������������������ʱ�տ���¼���߼�
                // =============================================================
                if (g_replay.state == REPLAY_RECORDING) {
                    // �������õ� Stride �������еȼ����ϡ����
                    if (current_it % g_replay.recordStride == 0) {
                        if (g_replay.frames.size() < static_cast<size_t>(g_replay.maxFrames)) {
                            std::vector<float> h_vz(ctx.total_grid);

                            // ����ԭ�е� copyWavefieldToHost �� GPU ����Ϻõ� Vz ������ CPU �ڴ�
                            copyWavefieldToHost(gpu_data, h_vz.data());
                            g_replay.frames.push_back(h_vz);
                        }
                        else {
                            // ¼�����Զ�תΪ����״̬
                            g_replay.state = REPLAY_IDLE;
                        }
                    }
                }

                current_it++;
            }
        }
    }
}

// =============================================================================
// ��������������ÿ֡�Զ����ڲɼ����������� (Symmetric Shot Gather Sequencer)
// =============================================================================
/** Advances the multi-shot acquisition state machine, resetting, stepping, and exporting each shot. */
inline void updateAutoAcquisitionWorkflow(SimState& state, GLHandles& gl) {
    if (g_autoAcq.state != AUTO_ACQ_RUNNING) return;

    // ��鵱ǰ�ڵ�ʱ��¼���Ƿ��Ѿ��ﵽ�趨���ܲ���ʱ��
    if (current_it >= ctx.nt) {
        // -----------------------------------------------------------------
        // 1. ��һ���������ݻ�¼����ִ���Զ��߱��� SEG-Y ���� (���������ͷ)
        // -----------------------------------------------------------------
        char filename[256];
        sprintf_s(filename, "%sshot_%03d.sgy", g_autoAcq.exportFolder.c_str(), g_autoAcq.currentShotIdx + 1);

        // ��ƽ̨��ȫ��������ļ��� (mkdir/ _mkdir ���� Common.h ͷ������)
        std::string folder = g_autoAcq.exportFolder;
        if (!folder.empty() && folder.back() != '/' && folder.back() != '\\') {
            folder += "/";
        }
#ifdef _WIN32
        _mkdir(folder.c_str());
#else
        mkdir(folder.c_str(), 0777);
#endif

        // ִ��˫���� (_vx, _vz) ͬ����Ч��д��
        exportToSegy(rec, filename);

        // -----------------------------------------------------------------
        // 2. �ۼӲ�����Ƿ���δ��������һ��
        // -----------------------------------------------------------------
        g_autoAcq.currentShotIdx++;

        if (g_autoAcq.currentShotIdx < g_autoAcq.totalShots) {
            // ������һ�ڵ� X ��������
            int next_x = g_autoAcq.startX + g_autoAcq.currentShotIdx * g_autoAcq.shotIntervalX;
            edit_src_x = std::clamp(next_x, ctx.npml + 5, ctx.NX - ctx.npml - 5);
            edit_src_z = g_autoAcq.shotDepthZ;

            // =================================================================
            // �������޸���������ת����һ��ǰ��ǿ�����¸������µĲ����ػ��Ӳ���
            //  ��ִ��һ����ϵͳ���������룬���������ʷ�ɲ���������
            // =================================================================
            triggerSingleShot(gl, state);

            // ���ý����� Host �˻����������������ɼ�
            rec.initBuffer();
            rec.isRecording = true;
            state.running = true;

            std::cout << "[Acquisition] Automatically moving to Shot " << g_autoAcq.currentShotIdx + 1
                << " at Grid X: " << edit_src_x << std::endl;
        }
        else {
            // -----------------------------------------------------------------
            // 3. ����Ԥ���ڵ�ȫ���ɼ���ϣ�
            // -----------------------------------------------------------------
            g_autoAcq.state = AUTO_ACQ_FINISHED;
            state.running = false;
            rec.isRecording = false;

            popup_message = "All " + std::to_string(g_autoAcq.totalShots) + " shots collected and exported successfully!\nFiles saved in folder: '" + folder + "'";
            show_success_popup = true;
        }
    }
}

// =============================================================================
// 28. OpenGL ��Ⱦȫ��������������
// =============================================================================
/** Maps the current CUDA wavefield texture onto the application viewport with aspect correction. */
inline void renderFullBackbufferWavefield(int winW, int winH, float barHeight, GLHandles& gl) {
    // 1. �Դ�ɫ��ӳ���� 2D copy д������
    CUDA_CHECK(cudaGraphicsMapResources(1, &gl.cudaSeisRes, 0));
    struct MappedResourceGuard {
        cudaGraphicsResource_t resource;
        ~MappedResourceGuard() {
            if (resource) cudaGraphicsUnmapResources(1, &resource, 0);
        }
    } mappedResourceGuard{gl.cudaSeisRes};
    CUDA_CHECK(cudaGraphicsSubResourceGetMappedArray(&gl.cudaSeisArray, gl.cudaSeisRes, 0, 0));

    uchar4*& d_rgba_out = gl.deviceWavefieldRgba;
    int& last_size = gl.wavefieldRgbaCapacity;

    if (last_size != ctx.total_grid) {
        if (d_rgba_out) cudaFree(d_rgba_out);
        d_rgba_out = nullptr;
        CUDA_CHECK(cudaMalloc(&d_rgba_out, static_cast<size_t>(ctx.total_grid) * sizeof(uchar4)));
        last_size = ctx.total_grid;
    }
    generateWavefieldTextureOnGpu(gpu_data, d_rgba_out, color_scale, show_component);

    CUDA_CHECK(cudaMemcpy2DToArray(
        gl.cudaSeisArray, 0, 0,
        d_rgba_out,
        ctx.NX * sizeof(uchar4),
        ctx.NX * sizeof(uchar4),
        ctx.NZ,
        cudaMemcpyDeviceToDevice
    ));
    CUDA_CHECK(cudaGraphicsUnmapResources(1, &gl.cudaSeisRes, 0));
    mappedResourceGuard.resource = nullptr;

    // 2. ��ɫ����Ⱦ����
    glViewport(0, 0, winW, winH);
    glUseProgram(gl.seisProg);

    // �󶨲�������𲨳���������Ԫ 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl.seisTex);
    glUniform1i(glGetUniformLocation(gl.seisProg, "seisTexture"), 0);

    // ���Ԫ 1 �����������ʲ�λ��������
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_modelTex);
    glUniform1i(glGetUniformLocation(gl.seisProg, "modelTexture"), 1);

    // ���ݵ��𼰵��ʱ�����ص� Uniform ����
    glUniform1i(glGetUniformLocation(gl.seisProg, "waveStyle"), waveStyle);
    glUniform1i(glGetUniformLocation(gl.seisProg, "modelStyle"), modelStyle); // ���ݱ������
    glUniform1i(glGetUniformLocation(gl.seisProg, "showGrid"), showGrid ? 1 : 0); // ���񿪹�״̬

    glUniform1f(glGetUniformLocation(gl.seisProg, "gridSpacing"), ui_gridSpacing);
    glUniform1f(glGetUniformLocation(gl.seisProg, "gridOpacity"), ui_gridOpacity);

    // �޷����´��ݵ�ǰ�������õ� Vp �� �ܶ� (Rho)����������������
    glUniform1f(glGetUniformLocation(gl.seisProg, "uniformVp"), edit_Vp);
    glUniform1f(glGetUniformLocation(gl.seisProg, "uniformRho"), edit_Density);
    glUniform1i(glGetUniformLocation(gl.seisProg, "useModelTexture"), 1); // ���÷Ǿ����ٶ�ģ������

    // �󶨳������ Uniform ����
    glUniform2f(glGetUniformLocation(gl.seisProg, "winSize"), (float)winW, (float)winH);
    glUniform2f(glGetUniformLocation(gl.seisProg, "simSize"), (float)ctx.NX, (float)ctx.NZ);
    glUniform2f(glGetUniformLocation(gl.seisProg, "viewOffset"), viewOffset.x, viewOffset.y);
    glUniform1f(glGetUniformLocation(gl.seisProg, "viewZoom"), viewZoom);
    glUniform1f(glGetUniformLocation(gl.seisProg, "totalTime"), (float)glfwGetTime());
    glUniform1f(glGetUniformLocation(gl.seisProg, "npml"), (float)ctx.npml);

    // �ٴΰ󶨲���������
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl.seisTex);
    glUniform1i(glGetUniformLocation(gl.seisProg, "seisTexture"), 0);

    glBindVertexArray(gl.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/// ////////////////////////////////////////////////////////////////////////��������
// =============================================================================
// 29. �����棺����ģ���άͼ���û������� HUD ������� (ImGui / ImPlot)
// =============================================================================
/** Draws simulation controls, run state, acquisition controls, and device telemetry over the wavefield. */
inline void renderSeisHud(SimState& state, int winW, int winH, float barHeight, float scale, GLHandles& gl, const GpuInfo& info) {
    ImGuiIO& io = ImGui::GetIO();

    // A. ����������������Ӧ��� (�ڲ���֮�ϣ�ImGui �������֮��)
    renderGridRulerOnBackbuffer(ctx, state, winW, winH, barHeight, viewOffset, viewZoom);

    // B. ���Ƹ����Ƽ������ (Fg ͼ��)
    ViewportInfo vp;
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.w = (float)winW;
    vp.h = (float)winH;
    vp.scaleX = vp.w / (float)ctx.NX;
    vp.scaleY = vp.h / (float)ctx.NZ;
    renderBrushCursor(state, vp);

    // C. ���Ƶ�ǰ�༭/������������Դ��ɫ/��ɫ׼��
    renderSourceMarker(ctx, edit_src_x, edit_src_z, winW, winH, barHeight, viewOffset, viewZoom);

    // --- ����������������ͼ�㡿 ---
    if (rec.isShowarray && !rec.receivers.empty()) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();

        float  winAspect = (float)winW / (float)winH;
        float  simAspect = (float)ctx.NX / (float)ctx.NZ;
        ImVec2 aspectCorr = { 1.0f, 1.0f };
        if (winAspect > simAspect) {
            aspectCorr.x = winAspect / simAspect;
        }
        else {
            aspectCorr.y = simAspect / winAspect;
        }

        for (size_t i = 0; i < rec.receivers.size(); ++i) {
            float aPos_x = ((float)rec.receivers[i].x / (float)ctx.NX) * 2.0f - 1.0f;
            float aPos_y = 1.0f - ((float)rec.receivers[i].y / (float)ctx.NZ) * 2.0f;

            float Pndc_x = (aPos_x - viewOffset.x) * viewZoom / aspectCorr.x;
            float screenX = (Pndc_x * 0.5f + 0.5f) * winW;

            float Pndc_y = (aPos_y - viewOffset.y) * viewZoom / aspectCorr.y;
            float screenY = (0.5f - Pndc_y * 0.5f) * winH;

            if (screenX >= 0.0f && screenX <= winW && screenY >= barHeight && screenY <= winH) {
                // ����ѧ����ͨ�õĻ�ɫ�������θ�����־�첨��
                ImVec2 p1(screenX, screenY - 5.0f);
                ImVec2 p2(screenX - 4.0f, screenY + 3.0f);
                ImVec2 p3(screenX + 4.0f, screenY + 3.0f);
                fg->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 230, 51, 230)); // ӫ���
                fg->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, 255), 1.0f);      // ��ڱ�
            }
        }
    }

    // -------------------------------------------------------------------------
    // D. ��Ⱦ���� TopBar ״̬ң����
    // -------------------------------------------------------------------------
    {
        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize({ (float)winW, barHeight });

        ImVec4 topBarBg = ImVec4(0.95f, 1.0f, 0.98f, 0.2f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, topBarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * scale, 0));

        ImGui::PushStyleColor(ImGuiCol_CheckMark, uiAccent);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(uiAccent.x * 0.15f, uiAccent.y * 0.15f, uiAccent.z * 0.15f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(uiAccent.x * 0.3f, uiAccent.y * 0.3f, uiAccent.z * 0.3f, 0.6f));

        ImGui::Begin("TopBar_Seis", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
        {
            float centerY = 14.0f * scale;

            ImGui::SetCursorPos({ 20 * scale, centerY });
            ImGui::TextDisabled("ACTIVE GPU:"); ImGui::SameLine();
            ImGui::TextColored(uiAccent, "%s", info.name);

            float physicalTime = current_it * ctx.dt;

            char computeTimeStr[64];
            if (accumulated_compute_time < 60.0f) {
                sprintf(computeTimeStr, "%.6f s", accumulated_compute_time);
            }
            else {
                int   minutes = (int)(accumulated_compute_time) / 60;
                float seconds = fmodf(accumulated_compute_time, 60.0f);
                sprintf(computeTimeStr, "%02dm %.2fs", minutes, seconds);
            }

            char stepStr[128];
            if (infinite_mode) {
                sprintf(stepStr, "%d / INF", current_it);
            }
            else {
                sprintf(stepStr, "%d / %d", current_it, ctx.nt);
            }

            char statusStr[512];
            sprintf(statusStr, "FORMULATION: TYPE %d  |  STEP: %s  |  PHYS TIME: %.4f s  |  COMPUTE TIME: %s",
                ctx.flag_type, stepStr, physicalTime, computeTimeStr);

            float textWidth = ImGui::CalcTextSize(statusStr).x;
            ImGui::SetCursorPos({ (winW - textWidth) * 0.5f, centerY });
            ImGui::TextColored(uiAccent, "%s", statusStr);

            ImGui::SetCursorPos({ (float)winW - 260.0f * scale, centerY - 2.0f * scale });

            static float displayFPS = 60.0f;
            displayFPS = displayFPS * 0.95f + io.Framerate * 0.05f;
            ImU32 fpsColor = IM_COL32(uiAccent.x * 255, uiAccent.y * 255, uiAccent.z * 255, 220);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(fpsColor), "[ %.0f FPS ]", displayFPS);

            ImGui::SameLine(0, 20 * scale);
            ImGui::SetCursorPosY(centerY - 5.0f * scale);
            ImGui::Checkbox("OSD_PANEL", &showHUD);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);

        ImDrawList* topDraw = ImGui::GetForegroundDrawList();
        topDraw->AddLine(
            { 0, barHeight },
            { (float)winW, barHeight },
            IM_COL32(uiAccent.x * 255, uiAccent.y * 255, uiAccent.z * 255, 60),
            1.0f
        );
    }

    // -------------------------------------------------------------------------
    // E. ��Ⱦ�������������� (LAB_CONTROLS)
    // -------------------------------------------------------------------------
    if (showHUD) {
        ImGui::SetNextWindowPos({ 30 * scale, 80 * scale }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({ 420 * scale, 780 * scale }, ImGuiCond_FirstUseEver);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.95f, 1.0f, 0.98f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Border, uiAccent);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, uiAccent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(uiAccent.x * 1.2f, uiAccent.y * 1.2f, uiAccent.z * 1.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabActive, uiAccent);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(uiAccent.x * 0.8f, uiAccent.y * 0.8f, uiAccent.z * 0.8f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, uiAccent);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(uiAccent.x * 0.8f, uiAccent.y * 0.8f, uiAccent.z * 0.8f, 0.8f));

        if (ImGui::Begin("Control Panel", &showHUD)) {
            static float displayFPS = 60.0f;
            displayFPS = displayFPS * 0.95f + io.Framerate * 0.05f;
            ImGui::TextColored(uiAccent, "FPS: %.1f", displayFPS);
            ImGui::Separator();

            if (ImGui::BeginTabBar("ControlTabs")) {

                // ==========================================
                // Tab 1: Simulation ��������
                // ==========================================
                if (ImGui::BeginTabItem("Simulation")) {
                    ImGui::Spacing();
                    ImGui::TextColored(uiAccent, "Simulation Protocol");
                    const char* sim_types[] = { "Type 1: Whole-domain PML", "Type 2: Interior FDM + Boundary PML", "Type 3: Free Surface + PML" };
                    int temp_type = ctx.flag_type - 1;
                    if (ImGui::Combo("Formulation", &temp_type, sim_types, IM_ARRAYSIZE(sim_types))) {
                        ctx.flag_type = temp_type + 1;
                        if (ctx.flag_type == 3 && ctx.upFlag && ctx.npml < kStencilRadius) {
                            ctx.npml = kStencilRadius;
                            temp_pml = kStencilRadius;
                            par.FDM.npml = static_cast<float>(kStencilRadius);
                            updatePmlDampingArrays();
                        }
                        g_resetSimRequested = true;
                    }
                    ImGui::Spacing();

                    ImGui::TextColored(uiAccent, "Time Step, Grid Spacing & Iteration Control");
                    float max_vp = 0.0f;
                    for (int k = 0; k < ctx.total_grid; ++k) {
                        float vp = sqrtf(ctx.lambda2mu[k] / ctx.rho[k]);
                        if (vp > max_vp) max_vp = vp;
                    }

                    ImGui::SliderFloat("Time Step (dt)", &temp_dt, 0.00001f, 0.003f, "%.6f s");
                    ImGui::SliderFloat("Grid Spacing (dx)", &temp_dx, 0.1f, 20.0f, "%.2f m");
                    // =========================================================================
                    // ���Ż��������� PML ���������� 0��ʵ��һ���޷��л���������ȫ����ģʽ��
                    // =========================================================================
                    const int minimumPmlWidth = ctx.flag_type == 3 && ctx.upFlag ? kStencilRadius : 0;
                    const int maximumPmlWidth = std::min(100, (std::min(ctx.NX, ctx.NZ) - 2 * kStencilRadius - 1) / 2);
                    temp_pml = std::clamp(temp_pml, minimumPmlWidth, maximumPmlWidth);
                    if (temp_pml == 0) {
                        ImGui::SliderInt("PML Layer Width", &temp_pml, minimumPmlWidth, maximumPmlWidth, "0 (Total Reflection)");
                    }
                    else {
                        ImGui::SliderInt("PML Layer Width", &temp_pml, minimumPmlWidth, maximumPmlWidth, "%d px");
                    }
                    ImGui::SliderInt("Max Steps (nt)", &temp_nt, 500, 50000);
                    ImGui::SliderInt("Steps / Frame", &steps_per_frame, 1, 100);

                    // =========================================================================
                    // ���ع�����˫��������CFL����ά�����ȶ��Է���ϵͳ
                    // =========================================================================
                    // 1. ���۾���ʧ�ȼ���ֵ��CFL ���ۿ����� = 0.601��
                    float cfl_theory = kCourantLimit2d;
                    float dt_theory_limit = cfl_theory * temp_dx / max_vp;

                    // 2. ʵ��Ӧ�ô�����İ�ȫ���ƣ�CFL ��ȫ������ = 0.480������ 20% ������ PML �߽緢ɢ��
                    float cfl_safe = kSafeCourant2d;
                    float dt_safe_limit = cfl_safe * temp_dx / max_vp;

                    ImGui::TextDisabled("CFL Stability Analysis:");

                    // ���ۼ�����ʾ
                    ImGui::Text("- Theoretical Limit (CFL = %.3f):", cfl_theory); ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%.6f s", dt_theory_limit);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Absolute mathematical threshold.\nGoing above this will cause instant numerical divergence (NaN).");
                    }

                    // ʵ�ʴ����󣨰�ȫ��������ʾ
                    ImGui::Text("- Processed Safe Limit (CFL = %.3f):", cfl_safe); ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "%.6f s", dt_safe_limit);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Engineering standard threshold with a 20%% safety margin.\nThis coefficient is practically applied to absorb PML boundary stiffness safely.");
                    }

                    ImGui::Text("- Current Draft Step (dt): %.6f s", temp_dt);

                    // 3. ��ɫ�߽��ȶ�״̬���ָʾ��
                    if (temp_dt <= dt_safe_limit) {
                        // ���ڰ�ȫ����
                        ImGui::TextColored({ 0.2f, 1.0f, 0.4f, 1.0f }, "[ STATUS: CFL STABLE (Highly Safe) ]");
                    }
                    else if (temp_dt <= dt_theory_limit) {
                        // �����ٽ�߷�������
                        ImGui::TextColored({ 1.0f, 0.62f, 0.0f, 1.0f }, "[ STATUS: CFL MARGINAL (Risk of PML boundary explosion!) ]");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Warning: The requested dt is mathematically stable for homogeneous grids,\nbut may diverge inside PML damping layers or free surfaces.");
                        }
                    }
                    else {
                        // �Ѿ�Խ�������ߣ��ض������ɢ
                        ImGui::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "[ STATUS: CFL DIVERGENT (Will blow up!) ]");
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("APPLY TIME & SPACING CONTROLS", { -1, 30 * scale })) {
                        // 1. ��ȡ��ǰ��������٣����ڰ�ȫ�߽�ǯλ���
                        float max_vp = 0.0f;
                        for (int k = 0; k < ctx.total_grid; ++k) {
                            float vp = sqrtf(ctx.lambda2mu[k] / ctx.rho[k]);
                            if (vp > max_vp) max_vp = vp;
                        }
                        if (max_vp <= 350.0f) max_vp = 2000.0f;

                        // 2. ������������ (CFL = 0.601)
                        float dt_theory_limit = kCourantLimit2d * temp_dx / max_vp;

                        // 3. ����ȫ�����������û������ dt �������ۼ��ޣ�ǿ�ƽ���ǯλ����Ϊ 0.480 �İ�ȫֵ
                        if (temp_dt > dt_theory_limit) {
                            temp_dt = kSafeCourant2d * temp_dx / max_vp;
                            popup_message = "Warning: Input dt exceeded CFL stability limit.\nAutomatically clamped to safe value.";
                            show_error_popup = true;
                        }

                        // 4. Ӧ�þ�����ȫ���صĲ�����ռ����
                        ctx.dt = temp_dt;
                        gpu_data.inv_dt = 1.0f / ctx.dt;
                        ctx.nt = temp_nt;
                        ctx.h = temp_dx;

                        par.model.dx = temp_dx;
                        par.model.dz = temp_dx;
                        ctx.npml = temp_pml;
                        par.FDM.npml = (float)temp_pml;

                        // 5. ���򻯲��ϵ������
                        ctx.c1_h = static_cast<float>(kStaggeredDerivativeWeights[0]) / temp_dx;
                        ctx.c2_h = static_cast<float>(kStaggeredDerivativeWeights[1]) / temp_dx;
                        ctx.c3_h = static_cast<float>(kStaggeredDerivativeWeights[2]) / temp_dx;
                        ctx.c4_h = static_cast<float>(kStaggeredDerivativeWeights[3]) / temp_dx;

                        // 6. ����ִ�и���Ч�����������������ɱ�������Ӧ����
                        updatePmlDampingArrays();

                        ctx.dp_flat.assign(ctx.NX, 0.0f);
                        int fs_idx = ctx.npml;
                        for (int j = 0; j < ctx.NX; ++j) {
                            int k = fs_idx * ctx.NX + j;
                            if (k < ctx.total_grid && ctx.lambda2mu[k] > 0.0f) {
                                float l2m = ctx.lambda2mu[k];
                                float lam = ctx.lambda[k];
                                ctx.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
                            }
                        }

                        // 7. ���·ֹܵ���¼�����ػ���Դ�Ӳ�
                        ctx.record_vx.assign(static_cast<size_t>(ctx.num_rcv) * ctx.nt, 0.0f);
                        ctx.record_vz.assign(static_cast<size_t>(ctx.num_rcv) * ctx.nt, 0.0f);
                        generateRickerWavelet(ctx.wavelet, ctx.nt, ctx.dt, par.FDM.f0, par.FDM.t0);

                        g_resetSimRequested = true;
                    }
                    ImGui::Spacing();

                    ImGui::Separator();
                    ImGui::TextColored(uiAccent, "GRID DIMENSIONS (NX, NZ)");
                    ImGui::PushItemWidth(100 * scale);
                    ImGui::InputInt("##WidthVal", &edit_w, 0, 0); ImGui::SameLine();
                    ImGui::PopItemWidth();
                    if (ImGui::Button("-##WDec")) { edit_w = std::max(64, edit_w - 64); } ImGui::SameLine();
                    if (ImGui::Button("+##WInc")) { edit_w = std::min(8192, edit_w + 64); } ImGui::SameLine();
                    ImGui::Text("Grid Width (NX)");
                    ImGui::Spacing();

                    ImGui::PushItemWidth(100 * scale);
                    ImGui::InputInt("##HeightVal", &edit_h, 0, 0); ImGui::SameLine();
                    ImGui::PopItemWidth();
                    if (ImGui::Button("-##HDec")) { edit_h = std::max(64, edit_h - 64); } ImGui::SameLine();
                    if (ImGui::Button("+##HInc")) { edit_h = std::min(8192, edit_h + 64); } ImGui::SameLine();
                    ImGui::Text("Grid Height (NZ)");

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 0.85f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                    if (ImGui::Button("APPLY NEW GRID SIZE & CLEAR MODEL", { -1, 35 * scale })) {
                        state.running = false;
                        current_it = 0;
                        accumulated_compute_time = 0.0f;
                        active_sources.clear();

                        par.model.xnum = edit_w;
                        par.model.znum = edit_h;

                        setupTestContext(ctx, par);
                        releaseGpuSimulation(gpu_data);

                        cudaGraphicsUnregisterResource(gl.cudaSeisRes);
                        glBindTexture(GL_TEXTURE_2D, gl.seisTex);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ctx.NX, ctx.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

                        initializeGpuSimulation(gpu_data, ctx);
                        first_align_needed = true;
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::Checkbox("V-Sync Control", &state.vsyncEnabled);
                    if (ImGui::IsItemDeactivatedAfterEdit()) glfwSwapInterval(state.vsyncEnabled ? 1 : 0);
                    ImGui::Spacing();

                    ImGui::Checkbox("Continuous Simulation (Infinite Mode)", &infinite_mode);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Check to run the wavefield tank indefinitely.\nIdeal for interactive sandbox clicking.");
                    }
                    ImGui::Spacing();

                    ImGui::Checkbox("Enable Multi-Source Real-time Clicks", &multi_source_mode);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Check to click and spawn multiple intersecting waves simultaneously\nUncheck for standard single-point reset mode.");
                    }
                    ImGui::Checkbox("Enable Show Monitor par", &show_Monitor_par);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Show additional diagnostic window for PML parameters and geophysical range.");
                    }

                    ImGui::Separator();
                    ImGui::TextColored(uiAccent, "EXECUTION CONTROLS");

                    if (state.running) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.6f, 0.1f, 0.85f));
                        if (ImGui::Button("PAUSE SIMULATION", { -1, 30 * scale })) {
                            state.running = false;
                        }
                        ImGui::PopStyleColor();
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.3f, 0.85f));
                        if (ImGui::Button("RUN SIMULATION", { -1, 30 * scale })) {
                            state.running = true;
                        }
                        ImGui::PopStyleColor();
                    }

                    if (ImGui::Button("RESET SIMULATION", { -1, 28 * scale })) {
                        g_resetSimRequested = true;
                    }

                    if (ImGui::Button("RESET VIEWPORT", { -1, 28 * scale })) {
                        g_resetViewportRequested = true;
                    }

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(uiAccent.x * 0.12f, uiAccent.y * 0.32f, uiAccent.z * 0.42f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(uiAccent.x * 0.18f, uiAccent.y * 0.52f, uiAccent.z * 0.62f, 1.0f));
                    if (ImGui::Button("OPEN DATA ANALYZER", { -1, 28 * scale })) {
                        g_analyzerState.isOpen = true; // �����ݷ��������Ӵ�
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::EndTabItem();
                }

                // ==========================================
                // Tab 2: Source ������Դ��Ƕȵ���
                // ==========================================
                if (ImGui::BeginTabItem("Source")) {
                    ImGui::Spacing();
                    ImGui::TextColored(uiAccent, "Source Position, Type & Amplitude");

                    int min_valid_grid = ctx.npml + 5;
                    int max_valid_x = ctx.NX - ctx.npml - 5;
                    int max_valid_z = ctx.NZ - ctx.npml - 5;

                    // 1. ����λ�õ���
                    ImGui::SliderInt("Source X (Grid)", &edit_src_x, min_valid_grid, max_valid_x);
                    ImGui::SliderInt("Source Z (Grid)", &edit_src_z, min_valid_grid, max_valid_z);
                    ImGui::Spacing();

                    // 2. ������������ֵ���ڻ�����
                    ImGui::SliderFloat("Excitation Amp", &edit_amp, 0.1f, 10.0f, "%.1f x");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Adjust the source intensity in real-time.");
                    }

                    // 3. ������������Դ��������ѡ�� Combo (����������ͼƬ������Ӧ)
                    const char* source_types[] = {
                        "Vertical Force (Tzz)",
                        "Horizontal Force (Txx)",
                        "Explosive (Txx + Tzz)",
                        "Shear (Txz)",
                        "Rotated Force"
                    };
                    ImGui::Combo("Source Type", &edit_source_type, source_types, IM_ARRAYSIZE(source_types));
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Select seismic source mechanism (e.g. Pure P-Wave or Shear S-Wave).");
                    }

                    // 4. �Ż�������ѡ����б�� (Rotated Force)��ʱ������ʾ�����Ƕȵ��ڣ������������
                    if (edit_source_type == 4) {
                        ImGui::SliderFloat("Force Angle", &edit_angle, -180.0f, 180.0f, "%.1f deg");
                    }

                    ImGui::Separator();
                    ImGui::TextColored(uiAccent, "Ricker Wavelet Properties");
                    ImGui::SliderFloat("Peak Freq (f0)", &edit_f0, 5.0f, 300.0f, "%.1f Hz");
                    ImGui::SliderFloat("Time Delay (t0)", &edit_t0, 0.001f, 0.2f, "%.3f s");

                    ImGui::TextDisabled("Waveform Real-Time Preview:");
                    float              preview_dt = 0.0005f;
                    std::vector<float> preview_x(200);
                    std::vector<float> preview_y(200);
                    for (int i = 0; i < 200; ++i) {
                        float t = i * preview_dt - edit_t0;
                        float pi2_f0 = M_PI * M_PI * edit_f0 * edit_f0;
                        preview_x[i] = i * preview_dt;
                        preview_y[i] = (1.0f - 2.0f * pi2_f0 * t * t) * expf(-pi2_f0 * t * t) * edit_amp; // Ԥ��ͬ��Ӧ�÷�ֵ
                    }

                    ImPlot::PushStyleColor(ImPlotCol_FrameBg, IM_COL32(125, 125, 125, 255));
                    ImPlot::PushStyleColor(ImPlotCol_PlotBg, IM_COL32(125, 125, 125, 255));
                    ImPlot::PushStyleColor(ImPlotCol_Line, IM_COL32(0, 255, 120, 255));
                    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, IM_COL32(35, 35, 35, 120));

                    if (ImPlot::BeginPlot("##WaveletPreview", ImVec2(-1, 95 * scale), ImPlotFlags_NoLegend)) {
                        ImPlot::SetupAxes("Time", "Amp", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);
                        ImPlot::SetupAxesLimits(0.0f, 200 * preview_dt, -1.1f * edit_amp, 1.1f * edit_amp);
                        ImPlot::PlotLine("Ricker", preview_x.data(), preview_y.data(), 200);
                        ImPlot::EndPlot();
                    }
                    ImPlot::PopStyleColor(4);

                    ImGui::Spacing();
                    if (ImGui::Button("APPLY SOURCE CONFIG", { -1, 30 * scale })) {
                        ctx.src_z_idx = edit_src_z;
                        ctx.src_idx = edit_src_z * ctx.NX + edit_src_x;
                        ctx.src_angle = edit_angle;
                        ctx.src_type = edit_source_type; // ���¼�������
                        ctx.src_amp = edit_amp;         // ���¼�����ֵ

                        par.FDM.f0 = edit_f0;
                        par.FDM.t0 = edit_t0;
                        par.FDM.angle = edit_angle;

                        generateRickerWavelet(ctx.wavelet, ctx.nt, ctx.dt, edit_f0, edit_t0);
                        current_it = 0;
                        state.running = false;
                        accumulated_compute_time = 0.0f;

                        releaseGpuSimulation(gpu_data);
                        initializeGpuSimulation(gpu_data, ctx);
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("TRIGGER SINGLE SOURCE SHOT (C)", { -1, 30 * scale })) {
                        multi_source_mode = false;
                        ctx.src_z_idx = edit_src_z;
                        ctx.src_idx = edit_src_z * ctx.NX + edit_src_x;
                        ctx.src_type = edit_source_type; // һ������ʱӦ�õ�ǰ����
                        ctx.src_amp = edit_amp;         // һ������ʱӦ�õ�ǰ��ֵ
                        g_resetSimRequested = true;
                    }
                    ImGui::Spacing();
                    ImGui::EndTabItem();
                }

                // ==========================================
                // Tab 3: Model_INPUT �ⲿ�������������
                // ==========================================
                if (ImGui::BeginTabItem("Model Edit")) {
                    ImGui::Spacing();
                    ImGui::TextColored(uiAccent, "Rock Physics Coupling");

                    ImGui::SliderFloat("P-Wave Velocity (Vp)", &edit_Vp, 500.0f, 6000.0f, "%.1f m/s");
                    ImGui::SliderFloat("S-Wave Velocity (Vs)", &edit_Vs, 200.0f, 3500.0f, "%.1f m/s");
                    ImGui::SliderFloat("Density (Rho)", &edit_Density, 1000.0f, 3000.0f, "%.1f kg/m^3");

                    if (edit_Vp < edit_Vs * 1.5f) edit_Vp = edit_Vs * 1.5f;

                    ImGui::Spacing();
                    if (ImGui::Button("APPLY PHYSICAL MEDIUM", { -1, 30 * scale })) {
                        float mu_val = edit_Density * edit_Vs * edit_Vs;
                        float lambda2mu_val = edit_Density * edit_Vp * edit_Vp;
                        float lambda_val = lambda2mu_val - 2.0f * mu_val;

                        ctx.rho.assign(ctx.total_grid, edit_Density);
                        ctx.mu.assign(ctx.total_grid, mu_val);
                        ctx.lambda.assign(ctx.total_grid, lambda_val);
                        ctx.lambda2mu.assign(ctx.total_grid, lambda2mu_val);
                        g_resetSimRequested = true;
                        updatePmlDampingArrays();
                        autoAlignTimeStep();
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(uiAccent, "GEOPHYSICAL SCENARIO PRESETS");

                    const char* scene_names[] = {
                        "Uniform Medium",
                        "Earth Shell & Core",
                        "Double Slit Interference",
                        "2-Layered Medium",
                        "Straight Waveguide",
                        "Curved Waveguide",
                        "Phononic Crystal Hex",
                        "Random Scattering",
                        "Sinusoidal Interface",
                        "Linear Velocity Gradient",
                        "Penrose Room",
                        "SCENE_TOPOGRAPHY"
                    };

                    static int selected_scene = current_scene;
                    ImGui::Combo("Select Preset", &selected_scene, scene_names, IM_ARRAYSIZE(scene_names));

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(uiAccent.x * 0.15f, uiAccent.y * 0.5f, uiAccent.z * 0.4f, 0.75f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(uiAccent.x * 0.2f, uiAccent.y * 0.7f, uiAccent.z * 0.5f, 0.9f));
                    ImGui::Spacing();
                    if (ImGui::Button("APPLY SCENARIO & REALLOCATE", { -1, 32 * scale })) {
                        current_scene = selected_scene;
                        applyScenario(current_scene, state);

                        edit_w = ctx.NX;
                        edit_h = ctx.NZ;

                        cudaGraphicsUnregisterResource(gl.cudaSeisRes);
                        glBindTexture(GL_TEXTURE_2D, gl.seisTex);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ctx.NX, ctx.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

                        first_align_needed = true;
                        g_resetSimRequested = true;
                    }
                    ImGui::Spacing();

                    // =========================================================================
                    // 1. ���������������������뼤�����������˵� (12345 ����)
                    // =========================================================================
                    ImGui::SeparatorText("Mouse Interaction Tool");

                    // �����˵���ʾ�ı����� ToolMode (0 ~ 4) �� 1��2��3��4��5 ����������
                    const char* brush_names[] = {
                        "1. Default (Trigger Source�� ",//Ĭ�ϼ�����Դ
                        "2. High Velocity Brush ",//(����Ӳ�һ���)
                        "3. Low Velocity Brush ",//(������㻭��)
                        "4. Custom Material Brush ",//(�Զ������Ի���)
                        "5. Eraser "//(��Ƥ�� / �ָ�����ɰ��)
                    };

                    int current_tool = state.brushType; // ��ȡ��ǰ״̬
                    ImGui::PushItemWidth(-1); // ���ȳ������Ӿ��ϸ�����

                    if (ImGui::Combo("##LeftClickToolCombo", &current_tool, brush_names, IM_ARRAYSIZE(brush_names))) {
                        state.brushType = current_tool; // ���ѡ��ʱͬ������״̬
                    }
                    ImGui::PopItemWidth();

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Choose what happens when you left-click on the wavefield.\nOr use hotkeys [1] to [5] on your keyboard.");
                    }

                    // =========================================================================
                    // 2. �������������ʴ�С���ڻ��� (����ѡ�񻭱�ģʽ 2, 3, 4, 5 ʱ�Զ�չʾ)
                    // =========================================================================
                    if (state.brushType != TOOL_NONE) {
                        ImGui::Spacing();
                        // ���� brushRadius �뾶��С (1 ~ 50 ���ص�Ԫ)
                        ImGui::SliderFloat("Brush Radius (px)", &state.brushRadius, 1.0f, 50.0f, "Radius: %.0f px");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Adjust the physical size of your paintbrush circle.");
                        }
                    }
                    ImGui::Spacing();
                    ImGui::PopStyleColor(2);
                    ImGui::EndTabItem();
                }
                // ==========================================
                // Tab 4: Model_EXPORT �ռ�ü��� SEGY ����
                // ==========================================
                if (ImGui::BeginTabItem("Model I/O")) {
                    // -------------------------------------------------------------
                    // �ⲿ�Զ����ı� (TXT) ģ�͸��ټ�����
                    // -------------------------------------------------------------
                    ImGui::Separator();
                    ImGui::TextColored(uiAccent, "EXTERNAL MODEL IMPORT (TXT)");

                    static char txt_file_path[512] = "";

                    ImGui::InputText("TXT File", txt_file_path, IM_ARRAYSIZE(txt_file_path));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##Txt", ImVec2(65 * scale, 0))) {
                        char filePath[512] = { 0 };
                        if (openSystemFileDialog(filePath, sizeof(filePath))) {
                            strcpy_s(txt_file_path, filePath);
                        }
                    }

                    static bool flipImportY = false;
                    ImGui::Checkbox("Flip Vertically on Import", &flipImportY);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Check this if your external model appears upside down.\nCommon for Python/Matlab exported data.");
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("LOAD CUSTOM MODEL FILE", { -1, 35 * scale })) {
                        if (strlen(txt_file_path) > 0) {
                            if (loadModelFromText(txt_file_path, flipImportY, gl, state)) {
                                popup_message = "External model loaded successfully!";
                                show_success_popup = true;
                                g_resetSimRequested = true;
                            }
                            else {
                                popup_message = "Failed to load model file.\nPlease check file formatting or dimensions.";
                                show_error_popup = true;
                            }
                        }
                        else {
                            popup_message = "Error: Please select a TXT model file first!";
                            show_error_popup = true;
                        }
                    }
                    ImGui::Spacing();

                    // -------------------------------------------------------------
                    // �ⲿ SEG-Y ��ά���Բ��߶��鵼�����̨
                    // -------------------------------------------------------------
                    ImGui::Separator();
                    ImGui::TextColored(uiAccent, "EXTERNAL MODEL IMPORT (SEG-Y GROUP)");

                    static char vp_file_path[512] = "";
                    static char vs_file_path[512] = "";
                    static char rho_file_path[512] = "";

                    ImGui::InputText("Vp (.sgy)", vp_file_path, IM_ARRAYSIZE(vp_file_path));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##Vp", ImVec2(65 * scale, 0))) {
                        char filePath[512] = { 0 };
                        if (openSystemFileDialog(filePath, sizeof(filePath))) {
                            strcpy_s(vp_file_path, filePath);
                        }
                    }

                    ImGui::InputText("Vs (.sgy)", vs_file_path, IM_ARRAYSIZE(vs_file_path));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##Vs", ImVec2(65 * scale, 0))) {
                        char filePath[512] = { 0 };
                        if (openSystemFileDialog(filePath, sizeof(filePath))) {
                            strcpy_s(vs_file_path, filePath);
                        }
                    }

                    ImGui::InputText("Rho(.sgy)", rho_file_path, IM_ARRAYSIZE(rho_file_path));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##Rho", ImVec2(65 * scale, 0))) {
                        char filePath[512] = { 0 };
                        if (openSystemFileDialog(filePath, sizeof(filePath))) {
                            strcpy_s(rho_file_path, filePath);
                        }
                    }

                    ImGui::SliderFloat("Original SEGY Spacing (dx)", &edit_segy_dx, 0.01f, 50.0f, "%.3f m");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("The original spatial spacing of your loaded SGY files.\nExample: 12.5m for standard Marmousi.");
                    }

                    static bool flipSegyY = true;
                    static bool flipSegyX = false;
                    ImGui::Checkbox("if (rho > 0.0f && rho < 10.0f) rho *= 1000.0f;", &convert_rho);
                    ImGui::Checkbox("Flip Vertically on SEGY Import", &flipSegyY);
                    ImGui::Checkbox("Flip Horizontally on SEGY Import", &flipSegyX);
                    ImGui::Spacing();

                    if (ImGui::Button("LOAD SEGY MODEL GROUP", { -1, 35 * scale })) {
                        if (strlen(vp_file_path) > 0 && strlen(vs_file_path) > 0 && strlen(rho_file_path) > 0) {
                            if (loadModelFromSegy(vp_file_path, vs_file_path, rho_file_path, flipSegyY, flipSegyX, gl, state)) {
                                popup_message = "SEGY Model Group loaded successfully!";
                                show_success_popup = true;
                                g_resetSimRequested = true;
                            }
                            else {
                                popup_message = "Failed to load SEGY files.\nPlease check file formatting or grid dimensions.";
                                show_error_popup = true;
                            }
                        }
                        else {
                            popup_message = "Error: Please select all three SGY files (Vp, Vs, Rho)!";
                            show_error_popup = true;
                        }
                    }
                   
                    ImGui::Separator();
                    ImGui::TextColored(uiAccent, "EXPORT CURRENT MODEL TO SEG-Y GROUP");

                    static bool export_density_gcm3 = true;
                    static bool export_flip_x = false;

                    static int crop_x_min = 0;
                    static int crop_z_min = 0;
                    static int crop_x_max = 0;
                    static int crop_z_max = 0;

                    if (crop_x_max <= 0 || crop_x_max >= ctx.NX || first_align_needed) {
                        crop_x_min = 0;
                        crop_z_min = 0;
                        crop_x_max = ctx.NX - 1;
                        crop_z_max = ctx.NZ - 1;
                    }

                    // ������Ӧ���롿��ÿ���л�ģ��ʱ���Զ���Ĭ�ϵ���Ŀ�경����Ϊ��ǰģ��Ĳ��� ctx.h
                    if (first_align_needed) {
                        export_target_dx = ctx.h;
                    }

                    ImGui::Checkbox("Convert Density to g/cm3 on Export", &export_density_gcm3);
                    ImGui::Checkbox("Flip Horizontally on Export", &export_flip_x);

                    ImGui::TextDisabled("Crop Sub-Region Coordinates (Grid Units):");
                    ImGui::SliderInt("X Min (Left)", &crop_x_min, 0, ctx.NX - 1);
                    ImGui::SliderInt("X Max (Right)", &crop_x_max, 0, ctx.NX - 1);
                    ImGui::SliderInt("Z Min (Top)", &crop_z_min, 0, ctx.NZ - 1);
                    ImGui::SliderInt("Z Max (Bottom)", &crop_z_max, 0, ctx.NZ - 1);

                    // =========================================================
                    // ���������������ز���Ŀ��ռ䲽�����ڻ���
                    // =========================================================
                    ImGui::SliderFloat("Export Target dx (m)", &export_target_dx, 0.1f, 20.0f, "%.2f m");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Target spatial spacing for exported SGY grid.\nSetting this different from active dx will trigger 2D Resampling.\ne.g., downsample Marmousi from 1.5m to 2.5m.");
                    }

                    float sub_btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;

                    if (ImGui::Button("SET TO FULL GRID", ImVec2(sub_btn_w, 0))) {
                        crop_x_min = 0;
                        crop_z_min = 0;
                        crop_x_max = ctx.NX - 1;
                        crop_z_max = ctx.NZ - 1;
                    }
                    ImGui::SameLine();

                    if (ImGui::Button("STRIP PML BOUNDARY", ImVec2(sub_btn_w, 0))) {
                        crop_x_min = ctx.npml;
                        crop_z_min = ctx.npml;
                        crop_x_max = ctx.NX - ctx.npml - 1;
                        crop_z_max = ctx.NZ - ctx.npml - 1;
                    }

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(uiAccent.x * 0.15f, uiAccent.y * 0.45f, uiAccent.z * 0.55f, 0.75f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(uiAccent.x * 0.2f, uiAccent.y * 0.65f, uiAccent.z * 0.75f, 0.9f));
                    if (ImGui::Button("EXPORT SEG-Y GROUP", { -1, 35 * scale })) {
                        char savePath[512] = { 0 };
                        if (saveSystemFileDialog(savePath, sizeof(savePath))) {
                            // �����²��� export_target_dx ���и߱����ز�������
                            if (exportModelToSegy(savePath, export_density_gcm3, export_flip_x, crop_x_min, crop_z_min, crop_x_max, crop_z_max, export_target_dx)) {
                                popup_message = "Cropped & Resampled SEGY Model Group exported successfully!\nCheck out your saved _vp, _vs, _rho files.";
                                show_success_popup = true;
                            }
                            else {
                                popup_message = "Failed to export model.\nPlease check target path permissions.";
                                show_error_popup = true;
                            }
                        }
                    }
                    ImGui::Spacing();
                    ImGui::PopStyleColor(2);
                    ImGui::EndTabItem();
                }

                // ==========================================
                // Tab 4: Data Export ���ݲɼ���۲����в���
                // ==========================================
                if (ImGui::BeginTabItem("Data Export")) {
                    ImGui::Spacing();

                    // ----------------------------------------------------------
                    // 1. �ɼ��뵼������
                    // ----------------------------------------------------------
                    ImGui::SeparatorText("Acquisition Settings");
                    ImGui::SliderFloat("Sampling Rate (Hz)", &rec.samplingRateHz, 10.0f, 10000.0f, "%.0f Hz");
                    ImGui::DragFloat("Record Duration (s)", &rec.totalDurationSec, 1.0f, 0.1f, 7200.0f, "%.2f s");
                    ImGui::DragFloat("Export Every (s)", &rec.exportIntervalSec, 1.0f, 0.1f, 3600.0f, "%.1f s");

                    // ���������ʾ
                    int expectedPoints = (int)(rec.samplingRateHz * rec.totalDurationSec);
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Target Samples: %d points/channel", expectedPoints);
                    ImGui::Spacing();

                    // ----------------------------------------------------------
                    // 2. �첨������ (���� X ��Χ����)
                    // ----------------------------------------------------------
                    ImGui::SeparatorText("Receiver Geometry");
                    ImGui::SliderInt("Channels", &rec.numChannels, 1, 200, "%d");

                    // ����Ӧ���뵱ǰ������������ߴ�
                    ImGui::DragIntRange2("X Range", &rec.startX, &rec.endX, 1.0f, 0, ctx.NX - 1, "Start: %d", "End: %d");
                    ImGui::SliderInt("Depth (Y)", &rec.receiverDepth, 0, ctx.NZ - 1);

                    if (ImGui::Button("DEPLOY LINEAR ARRAY", ImVec2(-1, 0))) {
                        rec.receivers.clear();
                        rec.initBuffer();

                        // �����û����õ� startX �� endX ��������
                        float rangeX = (float)(rec.endX - rec.startX);
                        float spacing = (rec.numChannels > 1) ? (rangeX / (rec.numChannels - 1)) : 0;

                        for (int i = 0; i < rec.numChannels; ++i) {
                            rec.receivers.push_back({ (int)(rec.startX + i * spacing), rec.receiverDepth });
                        }
                        // ͬ���첨���ռ����˵��Կ���
                        syncReceiversToGpu();
                    }
                    ImGui::Checkbox("Show/Hide LINEAR ARRAY", &rec.isShowarray);
                    ImGui::Spacing();

                    // ----------------------------------------------------------
                    // 3. ¼����һ����������
                    // ----------------------------------------------------------
                    ImGui::SeparatorText("Recording Control");

                    if (rec.isRecording) {
                        float progress = rec.totalTimer / rec.totalDurationSec;
                        char  progBuf[64];
                        sprintf(progBuf, "Progress: %.2f / %.2f s", rec.totalTimer, rec.totalDurationSec);
                        ImGui::ProgressBar(progress, ImVec2(-1, 30), progBuf);

                        if (ImGui::Button("STOP & SAVE NOW", ImVec2(-1, 40))) {
                            rec.isRecording = false;
                            state.running = false;

                            // �������ݣ�һ�ݱ�׼ SGY (����ͷ��������)��һ�� CSV �ı�
                            exportToSegy(rec, "manual_export.sgy");
                            exportToCsv(rec, "manual_export.csv");

                            popup_message = "Gather saved to 'manual_export.sgy' & 'manual_export.csv'";
                            show_success_popup = true;
                        }
                    }
                    else {
                        bool ready = !rec.receivers.empty() && (rec.receivers.size() == rec.numChannels);
                        if (!ready) ImGui::BeginDisabled();

                        float availW = ImGui::GetContentRegionAvail().x;
                        float spacingW = ImGui::GetStyle().ItemSpacing.x;

                        // ��ť A: �������ɼ� (��Ĭ¼�Ƶ�ǰʱ��ʱ�޵ĸ���)
                        if (ImGui::Button("START CAPTURE", ImVec2(availW * 0.4f, 45))) {
                            rec.totalTimer = 0.0f;
                            rec.exportTimer = 0.0f;
                            rec.timer = 0.0f;
                            rec.currentFileIndex = 1;
                            rec.initBuffer();

                            syncReceiversToGpu();
                            rec.isRecording = true;
                            state.running = true;
                        }
                        ImGui::SameLine();

                        // ��ť B: һ���Զ���������ʼȫ�Զ��ɼ� (������ɫ)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                        if (ImGui::Button("TRIGGER & ACQUIRE", ImVec2(availW * 0.6f - spacingW, 45))) {
                            rec.totalTimer = 0.0f;
                            rec.exportTimer = 0.0f;
                            rec.timer = 0.0f;
                            rec.currentFileIndex = 1;
                            rec.initBuffer();

                            // ��ղ����ض�λ��ָ��λ�÷�Դ����һ����ɼ첨���Դ�ͬ��
                            triggerSingleShot(gl, state);

                            rec.isRecording = true;
                            state.running = true;
                        }
                        ImGui::PopStyleColor(2);

                        if (!ready) {
                            ImGui::EndDisabled();
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Hint: Deploy receivers first.");
                        }
                    }

                    // =========================================================================
                    // 4. ȫ�Զ����ڵ���ѭ���ɼ�����̨ (Pre-stack Sequencer - ��ʵʱ�ӿ�ͬ��)
                    // =========================================================================
                    ImGui::SeparatorText("Automated Multi-Shot Gather");

                    ImGui::SliderInt("Total Shots", &g_autoAcq.totalShots, 1, 100);

                    // --- �������Ż� A��������ʵʱ���������϶���ʼ X ����ʱ����Ļ׼���Զ�ͬ��λ�� ---
                    if (ImGui::SliderInt("Start Shot X (Grid)", &g_autoAcq.startX, ctx.npml + 5, ctx.NX - ctx.npml - 5)) {
                        if (g_autoAcq.state == AUTO_ACQ_IDLE) {
                            edit_src_x = g_autoAcq.startX; // ʵʱ��������Ļ�Ǻ����̸����ƶ�
                        }
                    }

                    // --- �������Ż� B��������ʵʱ���������϶�������� Z ����ʱ����Ļ׼���Զ�����λ�� ---
                    if (ImGui::SliderInt("Shot Depth Z (Grid)", &g_autoAcq.shotDepthZ, ctx.npml + 5, ctx.NZ - ctx.npml - 5)) {
                        if (g_autoAcq.state == AUTO_ACQ_IDLE) {
                            edit_src_z = g_autoAcq.shotDepthZ; // ���ͬ�����£���Ļ������Ӧ
                        }
                    }

                    ImGui::SliderInt("Shot Spacing (Grid)", &g_autoAcq.shotIntervalX, 5, 200);

                    // ��̬�������һ�ڵ� X ���꣬��ֹ����Ԥ��
                    int end_shot_x = g_autoAcq.startX + (g_autoAcq.totalShots - 1) * g_autoAcq.shotIntervalX;
                    if (end_shot_x >= ctx.NX - ctx.npml - 5) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: End Shot X (%d) exceeds right boundary!", end_shot_x);
                    }
                    else {
                        ImGui::TextDisabled("Estimated End Shot X: %d (Within Boundary)", end_shot_x);
                    }

                    // �����ļ�����·������
                    static char export_dir[128] = "exports/";
                    ImGui::InputText("Export Folder", export_dir, IM_ARRAYSIZE(export_dir));
                    g_autoAcq.exportFolder = export_dir;

                    ImGui::Spacing();

                    // =========================================================================
                    // �������Ż� C��������һ���ֶ�������Ԥ����ť (PREVIEW & SYNC POSITION)
                    //  ������Զ��ض�λ��ǰ������Դ�������ʷ���ţ��ò�����������������ݻ�
                    // =========================================================================
                    if (g_autoAcq.state == AUTO_ACQ_IDLE) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(uiAccent.x * 0.10f, uiAccent.y * 0.20f, uiAccent.z * 0.30f, 0.65f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(uiAccent.x * 0.15f, uiAccent.y * 0.35f, uiAccent.z * 0.45f, 0.85f));
                        if (ImGui::Button("PREVIEW & SYNC POSITION", ImVec2(-1, 26 * scale))) {
                            edit_src_x = g_autoAcq.startX;
                            edit_src_z = g_autoAcq.shotDepthZ;

                            // һ����ɴ�ϵͳ�������벢����ע�伤��
                            triggerSingleShot(gl, state);
                        }
                        ImGui::PopStyleColor(2);
                        ImGui::Spacing();
                    }

                    if (g_autoAcq.state == AUTO_ACQ_RUNNING) {
                        // ���ƶ��ڽ�����
                        float progress = (float)g_autoAcq.currentShotIdx / g_autoAcq.totalShots;
                        char  acqBuf[64];
                        sprintf_s(acqBuf, "Acquiring: Shot %d / %d", g_autoAcq.currentShotIdx + 1, g_autoAcq.totalShots);
                        ImGui::ProgressBar(progress, ImVec2(-1, 30 * scale), acqBuf);

                        // ǿ����ֹ��ť
                        if (ImGui::Button("ABORT AUTO GATHER", ImVec2(-1, 30 * scale))) {
                            g_autoAcq.state = AUTO_ACQ_IDLE;
                            state.running = false;
                            rec.isRecording = false;
                        }
                    }
                    else {
                        bool ready_to_acquire = !rec.receivers.empty() && (rec.receivers.size() == rec.numChannels);
                        if (!ready_to_acquire) ImGui::BeginDisabled();

                        // ����/ӫ��ɫһ�����ڼ�����ȫ���ɼ�
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(uiAccent.x * 0.15f, uiAccent.y * 0.45f, uiAccent.z * 0.55f, 0.75f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(uiAccent.x * 0.2f, uiAccent.y * 0.65f, uiAccent.z * 0.75f, 0.9f));
                        if (ImGui::Button("RUN AUTO SHOT GATHER", ImVec2(-1, 40 * scale))) {
                            g_autoAcq.Start();
                            rec.initBuffer();

                            triggerSingleShot(gl, state);
                            rec.isRecording = true;
                            state.running = true;
                        }
                        ImGui::PopStyleColor(2);

                        if (!ready_to_acquire) {
                            ImGui::EndDisabled();
                        }
                    }
                    ImGui::EndTabItem();
                }

                // =============================================================
                // Tab 6: Replay (������Ӱ�ط��� - �Զ����������Ż���)
                // =============================================================
                if (ImGui::BeginTabItem("Replay")) {
                    ImGui::Spacing();

                    // ---------------------------------------------------------
                    // 1. ¼�ƿ�������� (Recording Controls)
                    // ---------------------------------------------------------
                    ImGui::SeparatorText("Recording");

                    ImGui::Columns(2, "rec_layout", false);
                    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.65f);

                    ImGui::SliderInt("Max Frames", &g_replay.maxFrames, 100, 1000);

                    if (g_replay.state == REPLAY_IDLE) {
                        ImGui::SliderInt("Rec Stride", &g_replay.recordStride, 1, 20, "Every %d frames");
                    }
                    else {
                        ImGui::BeginDisabled();
                        ImGui::SliderInt("Rec Stride", &g_replay.recordStride, 1, 20, "Every %d frames");
                        ImGui::EndDisabled();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Save 1 frame every N simulation steps.");
                    }

                    // �Կ�ϵͳ�������Դ���
                    size_t free_mem = 0, total_mem = 0;
                    float used_vram_mb = 0.0f, total_vram_mb = 0.0f;
                    if (cudaMemGetInfo(&free_mem, &total_mem) == cudaSuccess) {
                        used_vram_mb = static_cast<float>(total_mem - free_mem) / (1024.0f * 1024.0f);
                        total_vram_mb = static_cast<float>(total_mem) / (1024.0f * 1024.0f);
                    }
                    float estimatedSec = (float)(g_replay.maxFrames * g_replay.recordStride) * ctx.dt;

                    ImGui::TextColored(uiAccent, "GPU VRAM: %.1f / %.1f MB  |  Est. Duration: ~%.2f s", used_vram_mb, total_vram_mb, estimatedSec);

                    ImGui::NextColumn();
                    float btnH = 55.0f * scale;

                    if (g_replay.state == REPLAY_RECORDING) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));

                        if (ImGui::Button("STOP\nREC", ImVec2(-1, btnH))) {
                            g_replay.state = REPLAY_IDLE;
                        }
                        ImGui::PopStyleColor(3);

                        float time = (float)ImGui::GetTime();
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 0.5f + 0.5f * sinf(time * 10.0f)), "REC: %d f", g_replay.frames.size());
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.6f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.2f, 1.0f));

                        if (ImGui::Button("START\nREC", ImVec2(-1, btnH))) {
                            g_replay.clear();
                            g_replay.state = REPLAY_RECORDING;
                            state.running = true;
                        }
                        ImGui::PopStyleColor(3);

                        if (g_replay.frames.empty()) ImGui::TextDisabled("Buffer Empty");
                        else                         ImGui::Text("%d Frames", g_replay.frames.size());
                    }

                    ImGui::Columns(1);
                    ImGui::Spacing();

                    // ------------------------------------------------------
                    // 2. �طſ��� (Playback Controls)
                    // ------------------------------------------------------
                    ImGui::SeparatorText("Playback Control");

                    bool hasFrames = !g_replay.frames.empty();
                    if (!hasFrames) ImGui::BeginDisabled();

                    ImGui::SetNextItemWidth(150 * scale);
                    ImGui::SliderFloat("Speed", &g_replay.playbackSpeed, 0.1f, 5.0f, "%.1fx", ImGuiSliderFlags_Logarithmic);
                    ImGui::SameLine();

                    float ctrlBtnW = 70.0f * scale;

                    if (g_replay.state == REPLAY_PLAYING) {
                        if (ImGui::Button("PAUSE", ImVec2(ctrlBtnW, 0))) {
                            g_replay.state = REPLAY_PAUSED;
                        }
                    }
                    else {
                        if (ImGui::Button("PLAY", ImVec2(ctrlBtnW, 0))) {
                            g_replay.state = REPLAY_PLAYING;
                            state.running = false;
                        }
                    }

                    ImGui::SameLine();

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button("CLEAR", ImVec2(ctrlBtnW, 0))) {
                        g_replay.clear();
                    }
                    ImGui::PopStyleColor(2);

                    ImGui::SameLine();

                    // SAVE �Զ�ƴ�ӣ� _NXxNZ_vz.bin 
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                    if (ImGui::Button("SAVE", ImVec2(ctrlBtnW, 0))) {
                        char savePath[512] = { 0 };
                        if (saveSystemFileDialog(savePath, sizeof(savePath))) {
                            std::string path_str(savePath);
                            size_t dot_pos = path_str.find_last_of('.');
                            if (dot_pos != std::string::npos) {
                                path_str = path_str.substr(0, dot_pos);
                            }
                            // �����Զ�����������ǰ����ߴ� (ctx.NX x ctx.NZ) �Զ���װ����׺�У�
                            std::string final_path = path_str + "_" + std::to_string(ctx.NX) + "x" + std::to_string(ctx.NZ) + "_vz.bin";

                            if (g_replay.exportToBinary(final_path, 1)) {
                                popup_message = "Wavefield movie saved with dimensions!\nFile: " + final_path;
                                show_success_popup = true;
                            }
                            else {
                                popup_message = "Failed to save wavefield movie.";
                                show_error_popup = true;
                            }
                        }
                    }
                    ImGui::PopStyleColor(2);

                    if (hasFrames) {
                        char overlay[64];
                        sprintf(overlay, "%d / %d f", g_replay.currentFrameIndex + 1, (int)g_replay.frames.size());

                        int frame = g_replay.currentFrameIndex;
                        if (ImGui::SliderInt("Timeline", &frame, 0, (int)g_replay.frames.size() - 1, overlay)) {
                            g_replay.currentFrameIndex = frame;
                            g_replay.currentFrameFloat = (float)frame;
                            g_replay.state = REPLAY_PAUSED;
                            state.running = false;
                        }
                    }
                    else {
                        ImGui::ProgressBar(0.0f, ImVec2(-1, 0), "No Active Recording");
                    }

                    if (!hasFrames) ImGui::EndDisabled();

                    ImGui::Spacing();

                    // ------------------------------------------------------
                    // 3. �����ع��������ݵ�Ӱ�������̨ (�������׺������� & ����Ӧ��������)
                    // ------------------------------------------------------
                    ImGui::SeparatorText("Import Wavefield Movie");

                    static char movie_load_path[512] = ""; // ��̬·��������
                    static int  movie_load_w = 0;  // ���������� NX
                    static int  movie_load_z = 0;  // ���������� NZ

                    // A. �ļ�ѡ���� (Browse)
                    ImGui::InputText("Movie Path", movie_load_path, IM_ARRAYSIZE(movie_load_path));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##Mv", ImVec2(65 * scale, 0))) {
                        char openPath[512] = { 0 };
                        if (openSystemFileDialog(openPath, sizeof(openPath))) {
                            strcpy_s(movie_load_path, openPath);

                            // �����Զ���������߼�
                            int parsed_w = 0, parsed_z = 0;
                            if (parseDimensionsFromPath(std::string(movie_load_path), parsed_w, parsed_z)) {
                                movie_load_w = parsed_w; // �Զ����
                                movie_load_z = parsed_z; // �Զ����
                            }
                            else {
                                // ���δ�����ļ�����ʶ�𣬽���Ĭ�ϲ��õ�ǰ�����С��Ϊ����ֵ
                                movie_load_w = ctx.NX;
                                movie_load_z = ctx.NZ;
                            }
                        }
                    }

                    // B. ��ά��ֵ�༭������ (�����û��ֶ�����΢��)
                    ImGui::PushItemWidth(120 * scale);
                    ImGui::InputInt("Import NX", &movie_load_w, 0, 0); ImGui::SameLine();
                    ImGui::InputInt("Import NZ", &movie_load_z, 0, 0);
                    ImGui::PopItemWidth();

                    // C. �����뵼���ť
                    bool can_load_movie = (strlen(movie_load_path) > 0) && (movie_load_w > 0) && (movie_load_z > 0);
                    if (!can_load_movie) ImGui::BeginDisabled();

                    ImGui::Spacing();
                    if (ImGui::Button("CONFIRM LOAD MOVIE", ImVec2(-1, 30 * scale))) {
                        if (g_replay.importFromBinary(movie_load_path, movie_load_w, movie_load_z)) {
                            popup_message = "Wavefield movie imported and locked successfully!\nClick [PLAY] to watch.";
                            show_success_popup = true;
                            state.running = false; // �����Ӱ���Զ�ֹͣ��ǰʱ��

                            // =========================================================
                            // ����������Ӧ��ȫ���롿���������ĵ�Ӱ������ϵͳ��ǰ����һ�£�
                            //  ϵͳ��ȫ�Զ������Ų� GPU �����ġ���������ߴ粢��װ�Դ棬��ֹԽ�����ˣ�
                            // =========================================================
                            if (movie_load_w != ctx.NX || movie_load_z != ctx.NZ) {
                                std::cout << "[Replay IO] Triggering Auto-Reallocation for imported movie: "
                                    << movie_load_w << "x" << movie_load_z << std::endl;

                                edit_w = movie_load_w;
                                edit_h = movie_load_z;
                                par.model.xnum = edit_w;
                                par.model.znum = edit_h;

                                setupTestContext(ctx, par);
                                releaseGpuSimulation(gpu_data);

                                cudaGraphicsUnregisterResource(gl.cudaSeisRes);
                                glBindTexture(GL_TEXTURE_2D, gl.seisTex);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ctx.NX, ctx.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                                CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

                                initializeGpuSimulation(gpu_data, ctx);
                                first_align_needed = true;
                            }
                        }
                        else {
                            popup_message = "Failed to read binary stream.\nEnsure the parameters match the original export.";
                            show_error_popup = true;
                        }
                    }
                    if (!can_load_movie) ImGui::EndDisabled();

                    ImGui::EndTabItem();
                }

                // ==========================================
                // Tab 5: visual ɫ����Ⱦ�����������
                // ==========================================
                if (ImGui::BeginTabItem("Visual")) {
                    ImGui::Spacing();
                    ImGui::SliderFloat("Color Gain", &color_scale, 0.01f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

                    // ��ȫ����ͼƬ�е�רҵ�������������
                    const char* components[] = {
                        "Velocity Mag",
                        "Velocity X",
                        "Velocity Y",
                        "Stress Normal",
                        "Stress Shear",
                        "P-Wave (Div)",
                        "S-Wave (Curl)"
                    };
                    ImGui::Combo("Show Component", &show_component, components, IM_ARRAYSIZE(components));

                    ImGui::Spacing();

                    const char* style_types[] = {
                        "0_Seismic Bipolar",      // ��׼��������
                        "1_Coolwarm",             // Matplotlib ������ů
                        "2_PuOr",                 // �ϳ�˫�����
                        "3_Grayscale",            // �Ҷ����ͼ
                        "4_Inferno",              // ������������
                        "5_Plasma",               // ��֪�����޺�
                        "6_Viridis",              // Matplotlib ��׼
                        "7_Turbo",                // Google Turbo �߶�̬
                        "8_Magma Glow",           // ���ҷ�����
                        "9_Jet Rainbow",          // ����ʺ�
                        "10_Hot",                 // �Ȼ�ɫ��
                        "11_Cold",                // ����ɫ��
                        "12_Terrain Map"          // ��ò����ͼ
                    };
                    ImGui::Combo("Visual Style", &waveStyle, style_types, IM_ARRAYSIZE(style_types));
                    ImGui::Spacing();

                    const char* model_styles[] = {
                        "Dark Titanium",
                        "Geological Map",
                        "Grayscale Velocity",
                        "WaveStyle Synced",
                        "Scientific White"
                    };
                    ImGui::Combo("Geological Background", &modelStyle, model_styles, IM_ARRAYSIZE(model_styles));
                    ImGui::Spacing();

                    ImGui::Checkbox("Show Background Grid & Dots", &showGrid);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Toggle the visibility of the grid lines and glowing intersection dots.");
                    }

                    ImGui::SliderFloat("Grid Spacing (m)", &ui_gridSpacing, 10.0f, 200.0f);
                    ImGui::SliderFloat("Grid Opacity", &ui_gridOpacity, 0.0f, 1.0f);

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            // �ײ������ɫ��ɫ��
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 55 * scale);
            float avail_w = ImGui::GetContentRegionAvail().x;
            float reset_btn_w = 60.0f * scale;
            float picker_w = avail_w - reset_btn_w - ImGui::GetStyle().ItemSpacing.x;

            ImGui::PushItemWidth(picker_w);
            ImGui::ColorEdit3("##AccentColorPicker", (float*)&uiAccent, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Interface Hue Accent Color");
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset##Theme", ImVec2(reset_btn_w, 0))) {
                uiAccent = ImVec4(0.5f, 1.0f, 0.5f, 1.0f); // �ָ�ӫ����
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Reset UI Accent Color to Default Cyan");
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(8);
    }

    // -------------------------------------------------------------------------
    // F. ��������Ӵ� (Simulation & PML Monitor)
    // -------------------------------------------------------------------------
    if (show_Monitor_par) {
        ImGui::SetNextWindowPos(ImVec2(30 * scale, 80 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420 * scale, 520 * scale), ImGuiCond_FirstUseEver);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.08f, 0.1f, 0.85f));   // ��ɫ�ѽ𱳾�
        ImGui::PushStyleColor(ImGuiCol_Border, uiAccent);                             // ӫ��߿�
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

        ImGui::PushStyleColor(ImGuiCol_HeaderActive, uiAccent);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(uiAccent.x * 0.8f, uiAccent.y * 0.8f, uiAccent.z * 0.8f, 0.8f));

        if (ImGui::Begin("Simulation & PML Monitor", &show_Monitor_par)) {
            static float displayFPS = 60.0f;
            displayFPS = displayFPS * 0.95f + io.Framerate * 0.05f;
            ImGui::TextColored(uiAccent, "MONITOR STATE  |  FPS: %.1f", displayFPS);
            ImGui::Separator();

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("ACTIVE PARAMETERS MONITOR", ImGuiTreeNodeFlags_DefaultOpen)) {
                // ��������ɫ (��˸Ƶ�� 6Hz��ƽ�����Ҳ�ֵ)
                float time = (float)ImGui::GetTime();
                float alpha = 0.35f + 0.65f * (sinf(time * 6.0f) * 0.5f + 0.5f);
                ImVec4 warningCol = ImVec4(1.0f, 0.82f, 0.0f, alpha); // ����ӫ���

                ImGui::TextDisabled("Grid Dimensions:"); ImGui::SameLine();
                ImGui::Text("%d x %d (Total: %d)", ctx.NX, ctx.NZ, ctx.total_grid);

                ImGui::TextDisabled("Physical Scale:"); ImGui::SameLine();
                ImGui::Text("%.1f m x %.1f m", ctx.NX * ctx.h, ctx.NZ * ctx.h);

                // =========================================================================
                // �������޸��������� cudaMemGetInfo ��ʵʱ����Ӳ�� VRAM ռ�������
                // =========================================================================
                size_t free_mem = 0;
                size_t total_mem = 0;
                float used_vram_mb = 0.0f;
                float total_vram_mb = 0.0f;

                // ֱ���� CUDA ����ʱ�����������Ӳ����ѯ
                if (cudaMemGetInfo(&free_mem, &total_mem) == cudaSuccess) {
                    used_vram_mb = static_cast<float>(total_mem - free_mem) / (1024.0f * 1024.0f);
                    total_vram_mb = static_cast<float>(total_mem) / (1024.0f * 1024.0f);
                }

                // 1. չ��ϵͳ�ײ���ʵ���Դ����帺��
                ImGui::TextDisabled("System GPU VRAM:"); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "%.1f / %.1f MB", used_vram_mb, total_vram_mb);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("System-wide allocated VRAM on the active GPU (Current Used / Total Capacity).");
                }

                // 2. չ�ֵ�ǰ��ֵ����ϵ����+����Ӧ�����������ռ���� (��ѧ�����������㷨����)
                ImGui::TextDisabled("Grid VRAM (Est.):"); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "%.2f MB", cached_vram_mb);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Estimated memory allocated strictly for this finite-difference simulation grid.");
                }
                // 1. ���Զ������� NVML
                g_nvml.Load();

                unsigned int gpu_util = 0;
                unsigned int gpu_temp = 0;
                unsigned int gpu_power_mw = 0;
                bool         nvml_ok = false;

                // 2. ����ɹ����ӣ�ֱ�����Կ�������ȡʵʱӲ��״̬
                if (g_nvml.loaded) {
                    nvmlUtilization_t util{};
                    if (g_nvml.getUtil(g_nvml.devHandle, &util) == NVML_SUCCESS) {
                        gpu_util = util.gpu;
                    }
                    if (g_nvml.getTemp(g_nvml.devHandle, NVML_TEMPERATURE_GPU, &gpu_temp) == NVML_SUCCESS) {
                        // �ɹ���������¶�
                    }
                    if (g_nvml.getPower(g_nvml.devHandle, &gpu_power_mw) == NVML_SUCCESS) {
                        // �ɹ�����ʵʱ���� (����)
                    }
                    nvml_ok = true;
                }

                // -------------------------------------------------------------
                // 3. չ�� GPU Core ʵʱ����ռ����
                // -------------------------------------------------------------
                ImGui::TextDisabled("GPU Core Usage:"); ImGui::SameLine();
                if (nvml_ok) {
                    ImGui::TextColored(uiAccent, "%u %%", gpu_util);
                }
                else {
                    ImGui::TextDisabled("N/A (NVML Offline)");
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Active SM core utilization rate (same as Task Manager's CUDA chart).");
                }

                // -------------------------------------------------------------
                // 4. չ�� GPU ����ʵʱ�����¶�
                // -------------------------------------------------------------
                ImGui::TextDisabled("GPU Temperature:"); ImGui::SameLine();
                if (nvml_ok) {
                    // �¶Ⱦ��棺�������¶ȳ��� 82��C ʱ���Զ���ɫ��ʾ
                    ImVec4 tempColor = (gpu_temp >= 82) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : uiAccent;
                    ImGui::TextColored(tempColor, "%u oC", gpu_temp);
                }
                else {
                    ImGui::TextDisabled("N/A (NVML Offline)");
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Real-time GPU core temperature.");
                }

                // -------------------------------------------------------------
                // 5. չ�� GPU ʵʱ�˼���������
                // -------------------------------------------------------------
                ImGui::TextDisabled("GPU Power Draw:"); ImGui::SameLine();
                if (nvml_ok) {
                    ImGui::TextColored(uiAccent, "%.1f W", static_cast<float>(gpu_power_mw) / 1000.0f);
                }
                else {
                    ImGui::TextDisabled("N/A (NVML Offline)");
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Real-time GPU board power consumption (Watts).");
                }


                ImGui::TextDisabled("Active Spacing (dx):"); ImGui::SameLine();
                ImGui::Text("%.1f m", ctx.h);
                if (std::abs(temp_dx - ctx.h) > 1e-4f) {
                    ImGui::SameLine();
                    ImGui::TextColored(warningCol, "[Apply Pending]");
                }

                ImGui::TextDisabled("Active Time Step (dt):"); ImGui::SameLine();
                ImGui::Text("%.6f s", ctx.dt);
                if (std::abs(temp_dt - ctx.dt) > 1e-7f) {
                    ImGui::SameLine();
                    ImGui::TextColored(warningCol, "[Apply Pending]");
                }

                ImGui::TextDisabled("Active Max Steps (nt):"); ImGui::SameLine();
                ImGui::Text("%d steps (%.2f s simulated)", ctx.nt, ctx.nt * ctx.dt);
                if (temp_nt != ctx.nt) {
                    ImGui::SameLine();
                    ImGui::TextColored(warningCol, "[Apply Pending]");
                }

                ImGui::TextDisabled("Active PML Width:"); ImGui::SameLine();
                ImGui::Text("%d px", ctx.npml);
                if (temp_pml != ctx.npml) {
                    ImGui::SameLine();
                    ImGui::TextColored(warningCol, "[Apply Pending]");
                }

                int active_src_x = ctx.src_idx % ctx.NX;
                int active_src_z = ctx.src_idx / ctx.NX;
                ImGui::TextDisabled("Active Source Pos:"); ImGui::SameLine();
                ImGui::Text("(%d, %d)", active_src_x, active_src_z);
                if (edit_src_x != active_src_x || edit_src_z != active_src_z) {
                    ImGui::SameLine();
                    ImGui::TextColored(warningCol, "[Apply Pending]");
                }

                ImGui::Separator();
                ImGui::TextColored(uiAccent, "ACTIVE GEOPHYSICAL METRICS");

                ImGui::TextDisabled("P-Wave Velocity (Vp):"); ImGui::SameLine();
                ImGui::Text("%.1f m/s ~ %.1f m/s", cached_min_vp, cached_max_vp);

                ImGui::TextDisabled("S-Wave Velocity (Vs):"); ImGui::SameLine();
                ImGui::Text("%.1f m/s ~ %.1f m/s", cached_min_vs, cached_max_vs);

                ImGui::TextDisabled("Density Range (Rho):"); ImGui::SameLine();
                ImGui::Text("%.1f ~ %.1f kg/m^3", cached_min_rho, cached_max_rho);
            }

            ImGui::Spacing();
            ImGui::TextDisabled("PML Damping Profile (dx) Real-Time Monitor:");

            // ������ʱ X ����������
            std::vector<float> pml_x_axis(ctx.NX);
            for (int j = 0; j < ctx.NX; ++j) {
                pml_x_axis[j] = (float)j;
            }

            ImPlot::PushStyleColor(ImPlotCol_FrameBg, IM_COL32(10, 12, 15, 255));
            ImPlot::PushStyleColor(ImPlotCol_PlotBg, IM_COL32(0, 0, 0, 255));
            ImPlot::PushStyleColor(ImPlotCol_Line, IM_COL32(0, 255, 120, 255));
            ImPlot::PushStyleColor(ImPlotCol_AxisGrid, IM_COL32(35, 35, 35, 120));

            if (ImPlot::BeginPlot("##PmlProfilePlot", ImVec2(-1, 140 * scale), ImPlotFlags_NoLegend)) {
                ImPlot::SetupAxes("Grid Node Index", "Damping (Hz)", ImPlotAxisFlags_NoTickLabels, 0);

                float max_d = 1.0f;
                for (float val : ctx.dx) {
                    if (val > max_d) max_d = val;
                }
                ImPlot::SetupAxesLimits(0.0f, ctx.NX, -max_d * 0.05f, max_d * 1.1f);

                ImPlot::PlotLine("Damping dx", pml_x_axis.data(), ctx.dx.data(), ctx.NX);

                double pml_left_boundary = ctx.npml;
                double pml_right_boundary = ctx.NX - ctx.npml;

                ImVec2 left_p1 = ImPlot::PlotToPixels(ImPlotPoint(pml_left_boundary, -max_d * 0.05f));
                ImVec2 left_p2 = ImPlot::PlotToPixels(ImPlotPoint(pml_left_boundary, max_d * 1.1f));
                ImVec2 right_p1 = ImPlot::PlotToPixels(ImPlotPoint(pml_right_boundary, -max_d * 0.05f));
                ImVec2 right_p2 = ImPlot::PlotToPixels(ImPlotPoint(pml_right_boundary, max_d * 1.1f));

                ImPlot::GetPlotDrawList()->AddLine(left_p1, left_p2, IM_COL32(255, 140, 0, 180), 1.5f);
                ImPlot::GetPlotDrawList()->AddLine(right_p1, right_p2, IM_COL32(255, 140, 0, 180), 1.5f);

                ImPlot::EndPlot();
            }
            ImPlot::PopStyleColor(4);
            ImGui::Spacing();
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }

    renderAnalysisWindow(g_analyzerState);

    // -------------------------------------------------------------------------
    // G. ��Ⱦ�ײ�רҵ״̬����� (BottomBar)
    // -------------------------------------------------------------------------
    {
        ImGui::SetNextWindowPos({ 0, (float)winH - barHeight });
        ImGui::SetNextWindowSize({ (float)winW, barHeight });

        ImVec4 botBarBg = ImVec4(0.95f, 1.0f, 0.98f, 0.2f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, botBarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * scale, 0));

        ImGui::Begin("BottomBar_Seis", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
        {
            float centerY = 14.0f * scale;

            ImGui::SetCursorPos({ 20 * scale, centerY });
            if (hover_valid) {
                ImGui::TextDisabled("GRID:"); ImGui::SameLine();
                ImGui::TextColored(uiAccent, "X:%d, Z:%d", hover_mx, hover_my);
                ImGui::SameLine();
                ImGui::TextDisabled("| COORD:"); ImGui::SameLine();
                ImGui::TextColored(uiAccent, "X:%.1f m, Z:%.1f m", hover_mx * ctx.h, hover_my * ctx.h);
            }
            else {
                ImGui::TextDisabled("GRID MONITOR: STANDBY (HOVER OVER WAVEFIELD)");
            }

            char paramStr[256];
            if (hover_valid) {
                sprintf(paramStr, "P-WAVE SPEED (Vp): %.1f m/s  |  S-WAVE SPEED (Vs): %.1f m/s  |  DENSITY (Rho): %.1f kg/m^3", hover_Vp, hover_Vs, hover_Rho);
            }
            else {
                sprintf(paramStr, "ROCK MEDIUM MONITOR: ACTIVE");
            }

            float textWidth = ImGui::CalcTextSize(paramStr).x;
            ImGui::SetCursorPos({ (winW - textWidth) * 0.5f, centerY });

            if (hover_valid) {
                ImGui::TextColored(uiAccent, "%s", paramStr);
            }
            else {
                ImGui::TextDisabled("%s", paramStr);
            }

            float rightSectionWidth = 220.0f * scale;
            ImGui::SetCursorPos({ (float)winW - rightSectionWidth, centerY });
            ImGui::TextDisabled("STATUS:"); ImGui::SameLine();
            if (hover_valid) {
                ImGui::TextColored({ 0.2f, 1.0f, 0.4f, 1.0f }, "INDEX LINKED");
            }
            else {
                ImGui::TextColored({ 1.0f, 0.5f, 0.0f, 1.0f }, "CURSOR OUT");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);

        ImDrawList* botDraw = ImGui::GetForegroundDrawList();
        botDraw->AddLine(
            { 0, (float)winH - barHeight },
            { (float)winW, (float)winH - barHeight },
            IM_COL32(uiAccent.x * 255, uiAccent.y * 255, uiAccent.z * 255, 60),
            1.0f
        );
    }
}

// =============================================================================
// 30. GPU ������Ⱦ��������� (���Բ���ֵģ����Ļ������Ⱦ��ˮ��)
// =============================================================================
/** Runs one frame of the 2D seismic workspace: initialize, interact, compute, render, and present HUD. */
inline void renderSeisSimScreenGpu(SimState& state, int winW, int winH, GLHandles& gl, const GpuInfo& info) {
    // 1. ���ڴ����������ȼ��� OSD ���������붥��/�ײ��˵����߶�
    const float scale = (float)winW / 1920.0f;
    const float barHeight = 38.0f * scale;

    // 2. �ӳٳ�ʼ��
    initializeSeismicSimulation(gl);

    // 3. �ӿ�����Ӧ�����밲ȫ����
    applyCameraAutoAlignment(winW, winH, barHeight);

    // 4. �û����벶��
    handleSeismicInteractions(state, winW, winH, barHeight);

    // =========================================================================
    // �������ع���������ʱ���ƽ��벨���ط��߼��ķ�������
    // =========================================================================
    if (g_replay.state != REPLAY_PLAYING && g_replay.state != REPLAY_PAUSED) {
        // A. ����ģ�����״̬
        updateWavefieldSimulation(state);

        // --- �����������������ƽ�ʱ�ݵ���һ�У�ִ�ж���ѭ��״̬����� ---
        updateAutoAcquisitionWorkflow(state, gl);
    }
    else {
        // B. ���ڲ����طŻ���ͣ��ק״̬��ֹͣ�����ƽ�����Ϊ���ٸ����Կ��е� Vz ����
        ImGuiIO& io = ImGui::GetIO();

        if (g_replay.state == REPLAY_PLAYING && !g_replay.frames.empty()) {
            // ���ݲ��ű�������Ӧ�ƽ�ʱ����
            g_replay.currentFrameFloat += g_replay.playbackSpeed * (io.DeltaTime * 60.0f);
            g_replay.currentFrameIndex = static_cast<int>(g_replay.currentFrameFloat);

            if (g_replay.currentFrameIndex >= static_cast<int>(g_replay.frames.size())) {
                g_replay.currentFrameIndex = 0; // �Զ�ѭ������
                g_replay.currentFrameFloat = 0.0f;
            }
            g_replay.uploadFrameToGpu(g_replay.currentFrameIndex, gpu_data);
        }
        else if (g_replay.state == REPLAY_PAUSED && !g_replay.frames.empty()) {
            // ��ͣ��קʱ��������д��ǰ�϶��� Frame����֤�����ʱ���� 1:1 ��̬ʵʱˢ��
            g_replay.uploadFrameToGpu(g_replay.currentFrameIndex, gpu_data);
        }
    }

    // 6. ��ͼ��Ⱦ��ִ���Դ� 2D ������ OpenGL ȫ����ɫ��Ⱦ
    renderFullBackbufferWavefield(winW, winH, barHeight, gl);

    // 7. GUI/HUD ����
    renderSeisHud(state, winW, winH, barHeight, scale, gl, info);

    // 8. ģ̬����
    renderSeisPopups();
}
