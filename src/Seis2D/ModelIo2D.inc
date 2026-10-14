// Private implementation fragment, included once by Simulation2D.cpp.
// Select a legacy 2D model input path without changing simulation state.
inline bool openSystemFileDialog(char* buffer, int bufferSize) {
    memset(buffer, 0, bufferSize);

#ifdef _WIN32
    
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = bufferSize;
    ofn.lpstrFilter = "Model Data (*.txt;*.sgy;*.csv)\0*.txt;*.sgy;*.segy;*.csv\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1; 
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn);
#else
    
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
            buffer[len - 1] = '\0'; 
        }
        success = (strlen(buffer) > 0);
    }
    pclose(pipe);
    return success;
#endif
}

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

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    return GetSaveFileNameA(&ofn) == TRUE;
}
#else
inline bool saveSystemFileDialog(char* outPath, size_t maxPathLen) {
    return false;
}
#endif

// Resamples a 2D material grid to the requested dimensions using interpolation.
inline std::vector<std::vector<float>> resampleGrid2d(
    const std::vector<std::vector<float>>& input,
    float                                  target_dx,
    float                                  original_dx
) {
    if (input.empty() || input[0].empty()) return {};

    int old_nx = static_cast<int>(input.size());    
    int old_nz = static_cast<int>(input[0].size()); 

    float physical_width = (old_nx - 1) * original_dx;
    float physical_depth = (old_nz - 1) * original_dx;

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

            float vp = grid_vp[src_x][src_z];
            float vs = grid_vs[src_x][src_z];
            float rho = grid_rho[src_x][src_z];

            if (convertRho2D) {
                if (rho > 0.0f && rho < 10.0f) rho *= 1000.0f;
                if (rho < 10.0f) rho = 1000.0f;
            }

            if (in_x_pml || in_z_pml) {
                if (vs <= 0.0f && vp > 350.0f) {
                    vs = vp / 2.0f; 
                }
            }

            if (component_type == 0)      output[x][z] = vp;
            else if (component_type == 1) output[x][z] = vs;
            else                          output[x][z] = rho;
        }
    }
    return output;
}

// Loads a text material model, validates its dimensions, and refreshes model views.
inline bool loadModelFromText(const std::string& filename, bool flipVertically, GLHandles& gl, SimState& state) {
    std::cout << "[IO] Loading custom ASCII model: " << filename << " (Flip Y: " << (flipVertically ? "Yes" : "No") << ")" << std::endl;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[IO] Error: Could not open model file: " << filename << std::endl;
        return false;
    }

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

    std::vector<std::vector<float>> raw_vp(file_w, std::vector<float>(file_h, 0.0f));
    std::vector<std::vector<float>> raw_vs(file_w, std::vector<float>(file_h, 0.0f));
    std::vector<std::vector<float>> raw_rho(file_w, std::vector<float>(file_h, 0.0f));

    file.clear();
    file.seekg(0, std::ios::beg);

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

    float original_txt_dx = 1.0f;
    float target_system_dx = parameters2D.model.dx;

    auto grid_vp = resampleGrid2d(raw_vp, target_system_dx, original_txt_dx);
    auto grid_vs = resampleGrid2d(raw_vs, target_system_dx, original_txt_dx);
    auto grid_rho = resampleGrid2d(raw_rho, target_system_dx, original_txt_dx);

    int  current_npml = static_cast<int>(parameters2D.FDM.npml);
    auto vp_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 0); 
    auto vs_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 1); 
    auto rho_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 2); 

    state.running = false;
    currentIt2D = 0;
    elapsedRunWallTime2D = 0.0f;
    activeSources2D.clear();

    gridWidth2D = static_cast<int>(vp_pad.size());
    gridHeight2D = static_cast<int>(vp_pad[0].size());
    parameters2D.model.xnum = gridWidth2D;
    parameters2D.model.znum = gridHeight2D;

    setupTestContext(simulationContext2D, parameters2D);

    for (int x = 0; x < simulationContext2D.NX; ++x) {
        for (int z = 0; z < simulationContext2D.NZ; ++z) {
            float vp = vp_pad[x][z];
            float vs = vs_pad[x][z];
            float rho = rho_pad[x][z];
            int   tgtX = x;
            int   tgtZ = z;

            if (flipVertically) {
                tgtZ = simulationContext2D.NZ - 1 - z; 
            }
            setMaterialAt(tgtX, tgtZ, vp, vs, rho);
        }
    }

    simulationContext2D.src_z_idx = simulationContext2D.NZ / 4;
    simulationContext2D.src_idx = simulationContext2D.src_z_idx * simulationContext2D.NX + (simulationContext2D.NX / 2);
    editSrcX2D = simulationContext2D.NX / 2;
    editSrcZ2D = simulationContext2D.NZ / 4;

    updatePmlDampingArrays();
    autoAlignTimeStep();

    simulationContext2D.dp_flat.assign(simulationContext2D.NX, 0.0f);
    int fs_idx = simulationContext2D.npml;
    for (int j = 0; j < simulationContext2D.NX; ++j) {
        int k = fs_idx * simulationContext2D.NX + j;
        if (k < simulationContext2D.total_grid && simulationContext2D.lambda2mu[k] > 0.0f) {
            float l2m = simulationContext2D.lambda2mu[k];
            float lam = simulationContext2D.lambda[k];
            simulationContext2D.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
        }
    }

    releaseGpuSimulation(gpuData2D);
    cudaGraphicsUnregisterResource(gl.cudaSeisRes);
    glBindTexture(GL_TEXTURE_2D, gl.seisTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, simulationContext2D.NX, simulationContext2D.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

    initializeGpuSimulation(gpuData2D, simulationContext2D);

    updateModelTexture();
    recalculateCachedModelMetrics();
    firstAlignNeeded2D = true;

    std::cout << "[IO] Custom ASCII Model Loaded, Padded and Stabilized Successfully: " << simulationContext2D.NX << "x" << simulationContext2D.NZ << std::endl;
    return true;
}

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

    float original_segy_dx = editSegyDx2D;
    float target_system_dx = parameters2D.model.dx;

    auto grid_vp = resampleGrid2d(raw_vp, target_system_dx, original_segy_dx);
    auto grid_vs = resampleGrid2d(raw_vs, target_system_dx, original_segy_dx);
    auto grid_rho = resampleGrid2d(raw_rho, target_system_dx, original_segy_dx);

    int  current_npml = static_cast<int>(parameters2D.FDM.npml);
    auto vp_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 0); 
    auto vs_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 1); 
    auto rho_pad = padPmlToGrid2d(grid_vp, grid_vs, grid_rho, current_npml, 2); 

    int file_w = static_cast<int>(vp_pad.size());
    int file_h = static_cast<int>(vp_pad[0].size());

    state.running = false;
    currentIt2D = 0;
    elapsedRunWallTime2D = 0.0f;
    activeSources2D.clear();

    gridWidth2D = file_w;
    gridHeight2D = file_h;
    parameters2D.model.xnum = gridWidth2D;
    parameters2D.model.znum = gridHeight2D;

    setupTestContext(simulationContext2D, parameters2D);

    for (int x = 0; x < simulationContext2D.NX; ++x) {
        for (int z = 0; z < simulationContext2D.NZ; ++z) {
            float vp = vp_pad[x][z];
            float vs = vs_pad[x][z];
            float rho = rho_pad[x][z];

            int tgtX = x;
            if (flipHorizontally) tgtX = simulationContext2D.NX - 1 - x;

            int tgtZ = z;
            if (flipVertically)   tgtZ = simulationContext2D.NZ - 1 - z;

            setMaterialAt(tgtX, tgtZ, vp, vs, rho);
        }
    }

    simulationContext2D.src_z_idx = simulationContext2D.NZ / 4;
    simulationContext2D.src_idx = simulationContext2D.src_z_idx * simulationContext2D.NX + (simulationContext2D.NX / 2);
    editSrcX2D = simulationContext2D.NX / 2;
    editSrcZ2D = simulationContext2D.NZ / 4;

    simulationContext2D.dp_flat.assign(simulationContext2D.NX, 0.0f);
    int fs_idx = simulationContext2D.npml;
    for (int j = 0; j < simulationContext2D.NX; ++j) {
        int k = fs_idx * simulationContext2D.NX + j;
        if (k < simulationContext2D.total_grid && simulationContext2D.lambda2mu[k] > 0.0f) {
            float l2m = simulationContext2D.lambda2mu[k];
            float lam = simulationContext2D.lambda[k];
            simulationContext2D.dp_flat[j] = (l2m * l2m - lam * lam) / l2m;
        }
    }
    updatePmlDampingArrays();
    autoAlignTimeStep();
    
    releaseGpuSimulation(gpuData2D);
    cudaGraphicsUnregisterResource(gl.cudaSeisRes);
    glBindTexture(GL_TEXTURE_2D, gl.seisTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, simulationContext2D.NX, simulationContext2D.NZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    CUDA_CHECK(cudaGraphicsGLRegisterImage(&gl.cudaSeisRes, gl.seisTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));

    initializeGpuSimulation(gpuData2D, simulationContext2D);

    updateModelTexture();
    recalculateCachedModelMetrics();
    firstAlignNeeded2D = true;

    return true;
}

// Writes the current material model to SEG-Y with the selected axis metadata.
inline bool exportModelToSegy(
    const std::string& base_filepath,
    bool               density_gcm3,
    bool               flipHorizontally,
    int                x_min,
    int                z_min,
    int                x_max,
    int                z_max,
    float              target_dx 
) {
    if (simulationContext2D.total_grid <= 0) return false;

    x_min = std::clamp(x_min, 0, simulationContext2D.NX - 1);
    x_max = std::clamp(x_max, 0, simulationContext2D.NX - 1);
    z_min = std::clamp(z_min, 0, simulationContext2D.NZ - 1);
    z_max = std::clamp(z_max, 0, simulationContext2D.NZ - 1);

    if (x_min > x_max) std::swap(x_min, x_max);
    if (z_min > z_max) std::swap(z_min, z_max);

    int new_NX = x_max - x_min + 1;
    int new_NZ = z_max - z_min + 1;

    std::cout << "[IO] Preparing SEGY Export with Cropping..." << std::endl;
    std::cout << "     - Crop Range: X[" << x_min << " ~ " << x_max << "], Z[" << z_min << " ~ " << z_max << "]" << std::endl;
    std::cout << "     - Raw Crop Size: " << new_NX << "x" << new_NZ << std::endl;

    std::vector<std::vector<float>> cropped_vp(new_NX, std::vector<float>(new_NZ, 0.0f));
    std::vector<std::vector<float>> cropped_vs(new_NX, std::vector<float>(new_NZ, 0.0f));
    std::vector<std::vector<float>> cropped_rho(new_NX, std::vector<float>(new_NZ, 0.0f));

    for (int x = 0; x < new_NX; ++x) {
        
        int targetX = flipHorizontally ? (x_max - x) : (x_min + x);

        for (int z = 0; z < new_NZ; ++z) {
            int targetZ = z_min + z; 

            int k_cuda = targetZ * simulationContext2D.NX + targetX; 

            float rho_val = simulationContext2D.rho[k_cuda];
            float vp_val = 0.0f;
            float vs_val = 0.0f;

            if (rho_val > 0.0f) {
                vp_val = sqrtf(simulationContext2D.lambda2mu[k_cuda] / rho_val);
                vs_val = sqrtf(simulationContext2D.mu[k_cuda] / rho_val);
            }

            cropped_vp[x][z] = vp_val;
            cropped_vs[x][z] = vs_val;

            if (density_gcm3) cropped_rho[x][z] = rho_val / 1000.0f;
            else              cropped_rho[x][z] = rho_val;
        }
    }

    std::vector<std::vector<float>> final_vp, final_vs, final_rho;
    int export_NX = new_NX;
    int export_NZ = new_NZ;

    if (target_dx > 0.0f && std::abs(target_dx - simulationContext2D.h) > 1e-4f) {
        std::cout << "     - [Trigger] Resampling sub-grid: " << simulationContext2D.h << "m -> " << target_dx << "m" << std::endl;
        final_vp = resampleGrid2d(cropped_vp, target_dx, simulationContext2D.h);
        final_vs = resampleGrid2d(cropped_vs, target_dx, simulationContext2D.h);
        final_rho = resampleGrid2d(cropped_rho, target_dx, simulationContext2D.h);

        export_NX = static_cast<int>(final_vp.size());
        export_NZ = static_cast<int>(final_vp[0].size());
    }
    else {
        final_vp = cropped_vp;
        final_vs = cropped_vs;
        final_rho = cropped_rho;
    }

    size_t             export_total_grid = static_cast<size_t>(export_NX) * export_NZ;
    std::vector<float> flat_vp(export_total_grid);
    std::vector<float> flat_vs(export_total_grid);
    std::vector<float> flat_rho(export_total_grid);

    for (int x = 0; x < export_NX; ++x) {
        for (int z = 0; z < export_NZ; ++z) {
            size_t k_segy = (size_t)x * export_NZ + z; 
            flat_vp[k_segy] = final_vp[x][z];
            flat_vs[k_segy] = final_vs[x][z];
            flat_rho[k_segy] = final_rho[x][z];
        }
    }

    std::string clean_path = base_filepath;
    size_t      dot_pos = clean_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        clean_path = clean_path.substr(0, dot_pos);
    }
    std::string out_vp_path = clean_path + "_vp.sgy";
    std::string out_vs_path = clean_path + "_vs.sgy";
    std::string out_rho_path = clean_path + "_rho.sgy";

    float dummy_dt = 0.001f; 
    SeismicIO::writeSegyFile2D(flat_vp, export_NX, export_NZ, out_vp_path, dummy_dt);
    SeismicIO::writeSegyFile2D(flat_vs, export_NX, export_NZ, out_vs_path, dummy_dt);
    SeismicIO::writeSegyFile2D(flat_rho, export_NX, export_NZ, out_rho_path, dummy_dt);

    std::cout << "[IO] Resampled SEGY Model Group Export Completed successfully." << std::endl;
    std::cout << "     - Final Output Size: " << export_NX << " traces x " << export_NZ << " samples" << std::endl;
    return true;
}

// Loads recorded seismic traces from SEG-Y into the analysis state.

// Extracts optional grid dimensions encoded in a model filename.
inline bool parseDimensionsFromPath(const std::string& path, int& out_w, int& out_z) {
    
    size_t last_x = path.find_last_of("xX");
    if (last_x == std::string::npos) return false;

    size_t start_idx = path.find_last_of('_', last_x);
    if (start_idx == std::string::npos) {
        start_idx = path.find_last_of("\\/", last_x);
        if (start_idx == std::string::npos) start_idx = 0;
        else                                start_idx += 1;
    }
    else {
        start_idx += 1; 
    }

    size_t end_idx = path.find_first_of('_', last_x);
    if (end_idx == std::string::npos) {
        end_idx = path.find_last_of('.');
        if (end_idx == std::string::npos || end_idx < last_x) {
            end_idx = path.size();
        }
    }

    if (start_idx >= last_x || last_x >= end_idx) return false;

    std::string w_str = path.substr(start_idx, last_x - start_idx);
    std::string z_str = path.substr(last_x + 1, end_idx - last_x - 1);

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

// Synchronizes receiver coordinates with the GPU sampling buffers.
