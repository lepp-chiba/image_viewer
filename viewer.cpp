#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

// GLEW
#define GLEW_STATIC
#include <GL/glew.h>

// GLFW
#include <GLFW/glfw3.h>

// libtiff
#include <tiffio.h>

// <--- 画像情報をまとめる構造体 ---
struct ImageInfo {
    GLuint textureID;
    std::string filename;
    float minVal; // 正規化された最小輝度値 (0.0-1.0)
    float maxVal; // 正規化された最大輝度値 (0.0-1.0)
};

// <--- グローバル変数を更新 ---
std::vector<ImageInfo> g_images;
int g_currentImageIndex = 0;
bool g_autoContrastEnabled = true; // <--- 自動コントラスト調整機能のON/OFFフラグ
GLFWwindow* g_window = nullptr;

// 画像データ管理用
std::vector<GLuint> g_textureIDs;
std::vector<std::string> g_filenames;

// ウィンドウタイトルを更新
void updateWindowTitle() {
    if (g_images.empty()) return;
    std::string title = "TIFF Viewer: " + g_images[g_currentImageIndex].filename;
    title += " | Auto-Contrast: " + std::string(g_autoContrastEnabled ? "ON" : "OFF");
    title += " (Press 'A' to toggle)";
    glfwSetWindowTitle(g_window, title.c_str());
}

// キー入力コールバック
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    bool needsTitleUpdate = false;
    int newIndex = g_currentImageIndex;

    if (key == GLFW_KEY_RIGHT) {
        newIndex = (g_currentImageIndex + 1) % g_images.size();
    } else if (key == GLFW_KEY_LEFT) {
        newIndex = (g_currentImageIndex - 1 + g_images.size()) % g_images.size();
    } else if (key == GLFW_KEY_A) { // <--- 'A'キーで自動調整をトグル
        g_autoContrastEnabled = !g_autoContrastEnabled;
        needsTitleUpdate = true;
    }

    if (newIndex != g_currentImageIndex) {
        g_currentImageIndex = newIndex;
        needsTitleUpdate = true;
    }

    if (needsTitleUpdate) {
        updateWindowTitle();
    }
}


// 16bit TIFFファイルを読み込む関数
uint16_t* load_16bit_tiff(const char* filename, int& width, int& height) {
    TIFF* tif = TIFFOpen(filename, "r");
    if (!tif) {
        std::cerr << "Error: Could not open TIFF file: " << filename << std::endl;
        return nullptr;
    }

    uint32_t w, h;
    uint16_t bitsPerSample, samplesPerPixel;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);

    // 16bitグレースケール画像のみをサポート
    if (bitsPerSample != 16 || samplesPerPixel != 1) {
        std::cerr << "Error: Unsupported TIFF format. Only 16-bit grayscale is supported." << std::endl;
        std::cerr << "  File: " << filename << ", BitsPerSample: " << bitsPerSample << ", SamplesPerPixel: " << samplesPerPixel << std::endl;
        TIFFClose(tif);
        return nullptr;
    }

    width = w;
    height = h;

    uint16_t* image_data = new uint16_t[width * height];
    if (!image_data) {
        std::cerr << "Error: Could not allocate memory for image data." << std::endl;
        TIFFClose(tif);
        return nullptr;
    }

    // 1行ずつ読み込む
    for (uint32_t row = 0; row < h; ++row) {
        if (TIFFReadScanline(tif, &image_data[row * width], row) < 0) {
            std::cerr << "Error reading scanline " << row << " from " << filename << std::endl;
            delete[] image_data;
            TIFFClose(tif);
            return nullptr;
        }
    }
    
    TIFFClose(tif);
    return image_data;
}

// --- シェーダーヘルパー関数の実装 ---
GLuint compileShader(const char* shaderPath, GLenum type) {
    GLuint shader = glCreateShader(type);
    std::ifstream file(shaderPath, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "ERROR::SHADER::FILE_NOT_FOUND: " << shaderPath << std::endl;
        return 0;
    }
    std::stringstream stream;
    stream << file.rdbuf();
    file.close();
    std::string code = stream.str();
    
    const char* source = code.c_str();
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED: " << shaderPath << "\n" << infoLog << std::endl;
        return 0;
    }
    return shader;
}

GLuint createShaderProgram(const char* vsPath, const char* fsPath) {
    GLuint vs = compileShader(vsPath, GL_VERTEX_SHADER);
    GLuint fs = compileShader(fsPath, GL_FRAGMENT_SHADER);
    if (vs == 0 || fs == 0) return 0;
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <image1.tif> <image2.tif> ..." << std::endl;
        return -1;
    }

    // --- GLFWとGLEWの初期化 ---
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(800, 600, "TIFF Viewer", NULL, NULL);
    if (g_window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(g_window);
    glfwSetKeyCallback(g_window, key_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // --- シェーダのコンパイル ---
    GLuint shaderProgram = createShaderProgram("shaders/shader.vert", "shaders/shader.frag");

    // --- 頂点データ (画面全体を覆う四角形) ---
    float vertices[] = {
        // positions         // texture coords
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f, // top right
         1.0f, -1.0f, 0.0f,   1.0f, 0.0f, // bottom right
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, // bottom left
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f  // top left
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    // --- TIFF画像の読み込みとテクスチャの作成 ---
    for (int i = 1; i < argc; ++i) {
        int width, height;
        uint16_t* data = load_16bit_tiff(argv[i], width, height);
        
        if (data) {
            ImageInfo info;
            info.filename = std::string(argv[i]);

            // <--- ここから輝度スキャン処理 ---
            uint16_t min_pixel_val = 65535;
            uint16_t max_pixel_val = 0;
            long long total_pixels = (long long)width * height;
            for (long long j = 0; j < total_pixels; ++j) {
                if (data[j] < min_pixel_val) min_pixel_val = data[j];
                if (data[j] > max_pixel_val) max_pixel_val = data[j];
            }
            std::cout << "Loaded: " << argv[i] << " (" << width << "x" << height << ")"
                      << " | Raw Min/Max: " << min_pixel_val << " / " << max_pixel_val << std::endl;

            // スキャンした値を正規化してImageInfoに格納
            info.minVal = static_cast<float>(min_pixel_val) / 65535.0f;
            info.maxVal = static_cast<float>(max_pixel_val) / 65535.0f;
            
            // 最大と最小が同じ値の場合の0除算を避ける
            if (info.maxVal - info.minVal < 1e-6) {
                info.maxVal = info.minVal + 0.001f; // ごくわずかな範囲を持たせる
            }
            // <--- ここまで輝度スキャン処理 ---


            glGenTextures(1, &info.textureID);
            glBindTexture(GL_TEXTURE_2D, info.textureID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, width, height, 0, GL_RED, GL_UNSIGNED_SHORT, data);
            
            g_images.push_back(info);
            
            delete[] data;
        }
    }

    if (g_images.empty()) {
        std::cerr << "No valid TIFF images were loaded. Exiting." << std::endl;
        glfwTerminate();
        return -1;
    }

    updateWindowTitle();
    
     // <--- シェーダのuniform変数の場所を取得 ---
    glUseProgram(shaderProgram);
    GLint minValLoc = glGetUniformLocation(shaderProgram, "u_minVal");
    GLint maxValLoc = glGetUniformLocation(shaderProgram, "u_maxVal");

     // --- レンダリングループ ---
    while (!glfwWindowShouldClose(g_window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        
        // <--- 現在の画像に応じてuniform変数を設定 ---
        if (g_autoContrastEnabled) {
            // 自動調整ON：計算したmin/maxをシェーダに送る
            glUniform1f(minValLoc, g_images[g_currentImageIndex].minVal);
            glUniform1f(maxValLoc, g_images[g_currentImageIndex].maxVal);
        } else {
            // 自動調整OFF：全範囲(0.0-1.0)を指定して、元の表示に戻す
            glUniform1f(minValLoc, 0.0f);
            glUniform1f(maxValLoc, 1.0f);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_images[g_currentImageIndex].textureID);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    // --- クリーンアップ ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
    for (const auto& info : g_images) {
        glDeleteTextures(1, &info.textureID);
    }

    glfwTerminate();
    return 0;
}