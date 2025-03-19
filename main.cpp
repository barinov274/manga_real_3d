#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <filesystem>
#include <FreeImage.h>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace fs = std::filesystem;

std::vector<std::string> pageFiles;
GLuint frontCoverTexture, backCoverTexture, spineTexture;
GLuint leftPageTexture, rightPageTexture;
int currentPage = 1;
float angleX = 0.0f, angleY = 0.0f;
GLfloat paper_depth = 0.001f;
GLuint stack_texture;
GLuint VAO, VBO, shaderProgram;
int bookSize;
bool front_close = 0, back_close = 0;

const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D texture1;
    void main() {
        FragColor = texture(texture1, TexCoord);
    }
)";

GLuint loadTexture(const std::string& filename) {
    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(filename.c_str(), 0);
    FIBITMAP* dib = FreeImage_Load(fif, filename.c_str());
    dib = FreeImage_ConvertTo32Bits(dib);

    int width = FreeImage_GetWidth(dib);
    int height = FreeImage_GetHeight(dib);
    GLubyte* textureData = FreeImage_GetBits(dib);

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, textureData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    FreeImage_Unload(dib);
    return textureID;
}

void deleteTexture(GLuint textureID) {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

void initGeometry() {
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Allocating space for 11 quadrilaterals * 4 vertices * 5 numbers (position + texture)
    glBufferData(GL_ARRAY_BUFFER, 11 * 4 * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

GLuint createShaderProgram() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

void updateBookGeometry(int currentPage) {
    float spine_size = bookSize * paper_depth;
    float radius = spine_size / 2;
    float angle = -90.0f + (180.0f / bookSize) * currentPage;
    float radian = glm::radians(angle);
    float x = radius * cos(radian);
    float y = radius * sin(radian);
    float x_rev = -x; // x_rev = radius * cos(radian + M_PI)
    float y_rev = -y; // y_rev = radius * sin(radian + M_PI)

    std::vector<float> vertices;

    // Spine
    vertices.insert(vertices.end(), {x, -1.0f, y, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {x_rev, -1.0f, y_rev, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {x_rev, 1.0f, y_rev, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {x, 1.0f, y, 0.0f, 1.0f});

    // Back cover
    vertices.insert(vertices.end(), {-1.0f + x_rev, -1.0f, y_rev, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {x_rev, -1.0f, y_rev, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {x_rev, 1.0f, y_rev, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-1.0f + x_rev, 1.0f, y_rev, 1.0f, 1.0f});

    // Front cover
    vertices.insert(vertices.end(), {1.0f + x, -1.0f, y, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {x, -1.0f, y, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {x, 1.0f, y, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {1.0f + x, 1.0f, y, 0.0f, 1.0f});

    // Left page
    vertices.insert(vertices.end(), {-1.0f, -1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {0.0f, -1.0f, spine_size / 2, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {0.0f, 1.0f, spine_size / 2, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-1.0f, 1.0f, spine_size / 2, 0.0f, 1.0f});

    // Right page
    vertices.insert(vertices.end(), {0.0f, -1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {1.0f, -1.0f, spine_size / 2, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {1.0f, 1.0f, spine_size / 2, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {0.0f, 1.0f, spine_size / 2, 0.0f, 1.0f});

    // Left stack
    vertices.insert(vertices.end(), {-1.0f, -1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {-1.0f - x, -1.0f, -y, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {-1.0f - x, 1.0f, -y, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-1.0f, 1.0f, spine_size / 2, 0.0f, 1.0f});

    // Right stack
    vertices.insert(vertices.end(), {1.0f, -1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {1.0f - x_rev, -1.0f, -y_rev, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {1.0f - x_rev, 1.0f, -y_rev, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {1.0f, 1.0f, spine_size / 2, 0.0f, 1.0f});

    // Up right stack
    vertices.insert(vertices.end(), {x, 1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {x, 1.0f, -y_rev, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {1.0f - x_rev, 1.0f, -y_rev, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {1.0f, 1.0f, spine_size / 2, 0.0f, 1.0f});

    // Bottom right stack
    vertices.insert(vertices.end(), {x, -1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {x, -1.0f, -y_rev, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {1.0f - x_rev, -1.0f, -y_rev, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {1.0f, -1.0f, spine_size / 2, 0.0f, 1.0f});

    // Up left stack
    vertices.insert(vertices.end(), {x_rev, 1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {x_rev, 1.0f, -y, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {-1.0f - x, 1.0f, -y, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-1.0f, 1.0f, spine_size / 2, 0.0f, 1.0f});

    // Bottom left stack
    vertices.insert(vertices.end(), {x_rev, -1.0f, spine_size / 2, 0.0f, 0.0f});
    vertices.insert(vertices.end(), {x_rev, -1.0f, -y, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {-1.0f - x, -1.0f, -y, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-1.0f, -1.0f, spine_size / 2, 0.0f, 1.0f});

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
}

void loadImages(const std::string& directory, const std::string& direction) {
    for (const auto& entry : fs::directory_iterator(directory)) {
        pageFiles.push_back(entry.path().string());
    }
    std::sort(pageFiles.begin(), pageFiles.end());

    frontCoverTexture = loadTexture(pageFiles[0]);
    backCoverTexture = loadTexture(pageFiles[pageFiles.size() - 2]);
    spineTexture = loadTexture(pageFiles[pageFiles.size() - 1]);

    if(direction == "ltr"){
        std::sort(pageFiles.begin(), pageFiles.end(), std::greater<std::string>());
        currentPage = pageFiles.size()-3;
    }

    leftPageTexture = loadTexture(pageFiles[currentPage]);
    rightPageTexture = loadTexture(pageFiles[currentPage+1]);
    stack_texture = loadTexture("stack.png");

    bookSize = pageFiles.size();
}

void renderBook(GLuint shader) {
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(angleX), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(angleY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(4,3,2));

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(shader, "texture1"), 0);

    // Spine
    glBindTexture(GL_TEXTURE_2D, spineTexture);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Back cover
    if(!back_close){
        glBindTexture(GL_TEXTURE_2D, backCoverTexture);
        glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
    }

    // Front cover
    if(!front_close){
        glBindTexture(GL_TEXTURE_2D, frontCoverTexture);
        glDrawArrays(GL_TRIANGLE_FAN, 8, 4);
    }

    // Left page
    if(!front_close)
        glBindTexture(GL_TEXTURE_2D, leftPageTexture);
    else
        glBindTexture(GL_TEXTURE_2D, frontCoverTexture);
    if(!back_close)
        glDrawArrays(GL_TRIANGLE_FAN, 12, 4);

    // Right page
    if(!back_close)
        glBindTexture(GL_TEXTURE_2D, rightPageTexture);
    else
        glBindTexture(GL_TEXTURE_2D, backCoverTexture);
    if(!front_close)
        glDrawArrays(GL_TRIANGLE_FAN, 16, 4);

    // Stacks
    glBindTexture(GL_TEXTURE_2D, stack_texture);
    if(!back_close){
        glDrawArrays(GL_TRIANGLE_FAN, 20, 4); // Left stack
        glDrawArrays(GL_TRIANGLE_FAN, 36, 4); // Top left stack
        glDrawArrays(GL_TRIANGLE_FAN, 40, 4); // Bottm left stack
    }
    if(!front_close){
        glDrawArrays(GL_TRIANGLE_FAN, 24, 4); // Right stack
        glDrawArrays(GL_TRIANGLE_FAN, 28, 4); // Top right stack
        glDrawArrays(GL_TRIANGLE_FAN, 32, 4); // Bottom right stack
    }

}

void set_win_title(int currentPage, int bookSize, SDL_Window* window, std::string direction){
    std::string title = "3D Book Viewer";
    if(direction == "rtl")
        title = "3D Book Viewer - Page " + std::to_string(currentPage) + " - " + std::to_string(currentPage+1) + " / " + std::to_string(bookSize);
    else if(direction == "ltr")
        title = "3D Book Viewer - Page " + std::to_string(abs(currentPage-bookSize)) + " - " + std::to_string(abs(currentPage-1-bookSize)) + " / " + std::to_string(bookSize );
    SDL_SetWindowTitle(window, title.c_str());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: <program> <directory> [rtl | ltr] page_num" << std::endl;
        return -1;
    }
    std::string directory = argv[1];
    std::string direction = (argc > 2) ? argv[2] : "ltr";
    //int page_num = (argc > 3) ? std::stoi(argv[3]) : -1;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("3D Book Viewer", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL  | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);

    glewInit();
    FreeImage_Initialise();

    glEnable(GL_DEPTH_TEST);
    shaderProgram = createShaderProgram();
    initGeometry();
    loadImages(directory, direction);

    updateBookGeometry(currentPage);

    std::string title = "3D Book Viewer";
    SDL_SetWindowTitle(window, title.c_str());

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f/600.0f, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 8.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    bool running = true;
    SDL_Event event;
    int lastX = 0, lastY = 0;
    bool mouseDown = false;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: running = false; break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        int width = event.window.data1;
                        int height = event.window.data2;
                        glViewport(0, 0, width, height);
                        projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouseDown = true;
                        lastX = event.button.x;
                        lastY = event.button.y;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) mouseDown = false;
                    break;
                case SDL_MOUSEMOTION:
                    if (mouseDown) {
                        angleY += (event.motion.x - lastX) * 0.5f;
                        angleX += (event.motion.y - lastY) * 0.5f;
                        lastX = event.motion.x;
                        lastY = event.motion.y;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    if (event.wheel.y > 0) { // Прокрутка вверх - приближение
                        view = glm::translate(view, glm::vec3(0.0f, 0.0f, 0.1f));
                    } else if (event.wheel.y < 0) { // Прокрутка вниз - отдаление
                        view = glm::translate(view, glm::vec3(0.0f, 0.0f, -0.1f));
                    }
                    break;
                case SDL_KEYDOWN:{
                    switch (event.key.keysym.sym) {
                        case SDLK_RIGHT:
                            if(direction == "rtl"){
                                if(currentPage < bookSize-3)
                                    currentPage += 2;
                                else
                                    front_close=!front_close;
                            }

                            if (direction == "ltr"){
                                if(currentPage > 2)
                                    currentPage -= 2;
                                else
                                    back_close=!back_close;
                            }

                            deleteTexture(leftPageTexture);
                            deleteTexture(rightPageTexture);
                            leftPageTexture = loadTexture(pageFiles[currentPage]);
                            rightPageTexture = loadTexture(pageFiles[currentPage + 1]);
                            updateBookGeometry(currentPage);
                            set_win_title(currentPage, bookSize, window, direction);
                            break;
                        case SDLK_LEFT:
                            if(direction == "rtl"){
                                if(currentPage > 1)
                                    currentPage -= 2;
                                else
                                    back_close=!back_close;
                            }

                            if(direction == "ltr"){
                                if(currentPage < bookSize-3)
                                    currentPage += 2;
                                else
                                    front_close=!front_close;
                            }

                            deleteTexture(leftPageTexture);
                            deleteTexture(rightPageTexture);
                            leftPageTexture = loadTexture(pageFiles[currentPage]);
                            rightPageTexture = loadTexture(pageFiles[currentPage + 1]);
                            updateBookGeometry(currentPage);
                            set_win_title(currentPage, bookSize, window, direction);
                            break;
                        case SDLK_UP:
                            angleX = 0.0f;
                            angleY = 0.0f;
                            break;
                        case SDLK_f:
                            if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
                                SDL_SetWindowFullscreen(window, 0); // exit fullscreen mode
                            } else {
                                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN); // Enter fullscreen mode
                            }
                            break;
                        case SDLK_ESCAPE:
                            running=false;
                            break;
                    }
                }
                break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
        renderBook(shaderProgram);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    FreeImage_DeInitialise();
    return 0;
}
