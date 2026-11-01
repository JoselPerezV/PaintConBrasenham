#include <iostream>
#include <vector>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Shader de vertices: pasa la posicion tal cual a la pantalla
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos, 0.0, 1.0);\n"
"}\0";

// Shader de fragmentos: pinta con el color que le mandamos desde C++ (uColor),
// asi un mismo shader sirve para la grilla y para las celdas
const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec4 uColor;\n"
"void main()\n"
"{\n"
"   FragColor = uColor;\n"
"}\0";

// GLFW llama esta funcion cada vez que cambia el tamano de la ventana.
// La grilla se estira para ocupar toda la ventana
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Bresenham con parametro de decision.
// Recibe dos celdas y devuelve las celdas que hay en el camino entre ellas
std::vector<std::pair<int, int>> bresenhamLine(int x0, int y0, int x1, int y1)
{
    std::vector<std::pair<int, int>> puntos;

    // Si la linea es mas vertical que horizontal, se cambia x por y
    // para poder usar siempre el mismo caso
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep)
    {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }

    // Siempre se avanza de izquierda a derecha
    if (x0 > x1)
    {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0;
    int dy = std::abs(y1 - y0);

    int p = 2 * dy - dx;             // parametro de decision inicial
    int y = y0;
    int ystep = (y0 < y1) ? 1 : -1;  // y sube o baja

    for (int x = x0; x <= x1; x++)
    {
        // Se guarda la celda, devolviendo x e y a su lugar si se habian cambiado
        if (steep)
            puntos.push_back({ y, x });
        else
            puntos.push_back({ x, y });

        // p decide si en la siguiente columna y cambia o se queda igual
        if (p > 0)
        {
            y += ystep;
            p += 2 * dy - 2 * dx;
        }
        else
        {
            p += 2 * dy;
        }
    }

    return puntos;
}

int main()
{
    // Ventana y OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Grilla", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Error creando ventana GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    gladLoadGL();
    glViewport(0, 0, 800, 800);

    // Shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Direccion de uColor, para cambiar el color antes de cada dibujo
    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");

    // Grilla: N cuadros por lado, cada uno de tamano step (la ventana va de -1 a 1)
    const int N = 20;
    float step = 2.0f / N;

    std::vector<float> gridVertices;

    // Lineas verticales
    for (int i = 0; i <= N; i++) {
        float x = -1.0f + i * step;
        gridVertices.push_back(x); gridVertices.push_back(-1.0f);
        gridVertices.push_back(x); gridVertices.push_back(1.0f);
    }
    // Lineas horizontales
    for (int j = 0; j <= N; j++) {
        float y = -1.0f + j * step;
        gridVertices.push_back(-1.0f); gridVertices.push_back(y);
        gridVertices.push_back(1.0f);  gridVertices.push_back(y);
    }

    // Buffers de la grilla (los datos no cambian, se suben una sola vez)
    GLuint gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Celdas pintadas: 0 = vacia, 1 = pintada. La celda (i, j) esta en celdas[j * N + i]
    std::vector<int> celdas(N * N, 0);

    // Triangulos de las celdas pintadas (se arman a partir de la matriz)
    std::vector<float> celdaVertices;

    // Buffers de las celdas (vacios por ahora, se llenan cuando se pinta algo)
    GLuint celdaVAO, celdaVBO;
    glGenVertexArrays(1, &celdaVAO);
    glGenBuffers(1, &celdaVBO);
    glBindVertexArray(celdaVAO);
    glBindBuffer(GL_ARRAY_BUFFER, celdaVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Estado del trazo: ultima celda pintada y si el clic sigue presionado
    int ultimaI = -1, ultimaJ = -1;
    bool dibujando = false;

    // Loop principal
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Inicio del frame de ImGui y panel
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Panel");
        ImGui::Text("Hola ImGui");
        ImGui::End();

        // Mouse: pinta celdas mientras se mantiene el clic izquierdo
        bool cambio = false;  // true si hay que reconstruir los cuadritos

        // No se pinta si el mouse esta sobre el panel de ImGui
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            int w, h;
            glfwGetWindowSize(window, &w, &h);

            // Posicion del mouse a celda (la y se invierte porque en la ventana crece hacia abajo)
            int i = (int)(mx / w * N);
            int j = (int)((h - my) / h * N);

            if (i >= 0 && i < N && j >= 0 && j < N)
            {
                if (!dibujando)
                {
                    // Primer clic del trazo: una sola celda
                    celdas[j * N + i] = 1;
                    cambio = true;
                }
                else if (i != ultimaI || j != ultimaJ)
                {
                    // El mouse cambio de celda: Bresenham rellena el camino desde la anterior
                    std::vector<std::pair<int, int>> camino = bresenhamLine(ultimaI, ultimaJ, i, j);
                    for (auto& c : camino)
                    {
                        celdas[c.second * N + c.first] = 1;
                    }
                    cambio = true;
                }

                ultimaI = i;
                ultimaJ = j;
                dibujando = true;
            }
        }
        else
        {
            // Sin clic, el trazo termino
            dibujando = false;
        }

        // Si se pinto algo, se rearman los cuadritos y se suben a la GPU
        if (cambio)
        {
            celdaVertices.clear();

            for (int j = 0; j < N; j++)
            {
                for (int i = 0; i < N; i++)
                {
                    if (celdas[j * N + i] == 0) continue;

                    // Esquinas de la celda, con la misma formula de la grilla
                    float x0c = -1.0f + i * step;
                    float y0c = -1.0f + j * step;
                    float x1c = x0c + step;
                    float y1c = y0c + step;

                    // Cada cuadrito son dos triangulos
                    celdaVertices.push_back(x0c); celdaVertices.push_back(y0c);
                    celdaVertices.push_back(x1c); celdaVertices.push_back(y0c);
                    celdaVertices.push_back(x0c); celdaVertices.push_back(y1c);

                    celdaVertices.push_back(x1c); celdaVertices.push_back(y0c);
                    celdaVertices.push_back(x1c); celdaVertices.push_back(y1c);
                    celdaVertices.push_back(x0c); celdaVertices.push_back(y1c);
                }
            }

            // DYNAMIC porque estos datos cambian seguido
            glBindBuffer(GL_ARRAY_BUFFER, celdaVBO);
            glBufferData(GL_ARRAY_BUFFER, celdaVertices.size() * sizeof(float), celdaVertices.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        // Dibujo: primero la grilla en blanco, luego las celdas en azul encima
        glUseProgram(shaderProgram);

        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, gridVertices.size() / 2);

        glUniform4f(colorLoc, 0.3f, 0.5f, 1.0f, 1.0f);
        glBindVertexArray(celdaVAO);
        glDrawArrays(GL_TRIANGLES, 0, celdaVertices.size() / 2);
        glBindVertexArray(0);

        // ImGui se dibuja al final para que el panel quede encima de todo
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Limpieza: se libera lo que se reservo en la GPU y se cierra todo
    glDeleteVertexArrays(1, &celdaVAO);
    glDeleteBuffers(1, &celdaVBO);
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteProgram(shaderProgram);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}