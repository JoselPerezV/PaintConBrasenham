#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Shader de vertices: pasa la posicion a la pantalla y le pasa el color del vertice al shader de fragmentos
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec3 aColor;\n"
"out vec3 vColor;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos, 0.0, 1.0);\n"
"   vColor = aColor;\n"
"}\0";

// Shader de fragmentos: si uUsarColorVertice es 1 pinta con el color de cada vertice (celdas),
// si es 0 pinta con el color fijo uColor (grilla)
const char* fragmentShaderSource = "#version 330 core\n"
"in vec3 vColor;\n"
"out vec4 FragColor;\n"
"uniform vec4 uColor;\n"
"uniform bool uUsarColorVertice;\n"
"void main()\n"
"{\n"
"   if (uUsarColorVertice)\n"
"       FragColor = vec4(vColor, 1.0);\n"
"   else\n"
"       FragColor = uColor;\n"
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

// Relleno por inundacion: parte de la celda (i, j) y pinta todas las celdas
// conectadas que sean iguales a ella (vacias, o pintadas del mismo color)
void rellenar(std::vector<int>& celdas, std::vector<float>& colores, int N, int i, int j, const float color[3])
{
    int inicio = j * N + i;
    int estado = celdas[inicio];

    // Color original de la celda donde se hizo clic
    float cr = colores[inicio * 3 + 0];
    float cg = colores[inicio * 3 + 1];
    float cb = colores[inicio * 3 + 2];

    // Si ya esta pintada con el color elegido, no hay nada que hacer
    if (estado == 1 && cr == color[0] && cg == color[1] && cb == color[2]) return;

    std::vector<bool> visitada(N * N, false);
    std::vector<std::pair<int, int>> pila;

    pila.push_back({ i, j });
    visitada[inicio] = true;

    // Mientras queden celdas pendientes: se saca una, se pinta y se revisan sus 4 vecinos
    while (!pila.empty())
    {
        int ci = pila.back().first;
        int cj = pila.back().second;
        pila.pop_back();

        int k = cj * N + ci;
        celdas[k] = 1;
        colores[k * 3 + 0] = color[0];
        colores[k * 3 + 1] = color[1];
        colores[k * 3 + 2] = color[2];

        // Vecinos: derecha, izquierda, arriba y abajo.
        // No se usan diagonales para que el relleno no se escape por las lineas de Bresenham
        int di[4] = { 1, -1, 0, 0 };
        int dj[4] = { 0, 0, 1, -1 };

        for (int d = 0; d < 4; d++)
        {
            int ni = ci + di[d];
            int nj = cj + dj[d];

            // Fuera de la grilla
            if (ni < 0 || ni >= N || nj < 0 || nj >= N) continue;

            int nk = nj * N + ni;

            // Ya revisada
            if (visitada[nk]) continue;

            // Solo se extiende por celdas iguales a la inicial
            if (celdas[nk] != estado) continue;
            if (estado == 1 && (colores[nk * 3 + 0] != cr || colores[nk * 3 + 1] != cg || colores[nk * 3 + 2] != cb)) continue;

            visitada[nk] = true;
            pila.push_back({ ni, nj });
        }
    }
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

    // Direcciones de las variables del shader, para cambiarlas antes de cada dibujo
    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    GLint usarVerticeLoc = glGetUniformLocation(shaderProgram, "uUsarColorVertice");

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

    // Color de cada celda (rojo, verde y azul), en el mismo orden que celdas
    std::vector<float> colores(N * N * 3, 0.0f);

    // Triangulos de las celdas pintadas. Cada vertice son 5 numeros: x, y, rojo, verde, azul
    std::vector<float> celdaVertices;

    // Buffers de las celdas (vacios por ahora, se llenan cuando se pinta algo)
    GLuint celdaVAO, celdaVBO;
    glGenVertexArrays(1, &celdaVAO);
    glGenBuffers(1, &celdaVBO);
    glBindVertexArray(celdaVAO);
    glBindBuffer(GL_ARRAY_BUFFER, celdaVBO);
    // Posicion: 2 numeros desde el inicio de cada vertice
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color: 3 numeros, despues de los 2 de la posicion
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Estado del trazo: ultima celda pintada y si el clic sigue presionado
    int ultimaI = -1, ultimaJ = -1;
    bool dibujando = false;

    // Herramienta activa: 0 = pincel, 1 = borrador, 2 = relleno
    int herramienta = 0;

    // Color con el que se pinta ahora (el que se elige en el panel)
    float colorActual[3] = { 0.3f, 0.5f, 1.0f };

    // Loop principal
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Inicio del frame de ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // true si hay que reconstruir los cuadritos
        bool cambio = false;

        // Panel de controles
        ImGui::Begin("Panel");

        // Selector de color: al elegir uno, las celdas nuevas se pintan con ese color
        ImGui::ColorEdit3("Color", colorActual);

        // Botones para elegir la herramienta
        ImGui::RadioButton("Pincel", &herramienta, 0);
        ImGui::RadioButton("Borrador", &herramienta, 1);
        ImGui::RadioButton("Relleno", &herramienta, 2);

        // Si se presiona, todas las celdas quedan vacias
        if (ImGui::Button("Borrar todo"))
        {
            std::fill(celdas.begin(), celdas.end(), 0);
            cambio = true;
        }

        ImGui::End();

        // Mouse: usa la herramienta elegida mientras se mantiene el clic izquierdo.
        // No hace nada si el mouse esta sobre el panel de ImGui
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            // Si la herramienta es el borrador se escribe 0 (vacia), si no 1 (pintada)
            int valor = (herramienta == 1) ? 0 : 1;

            // Marca una celda con el valor y le guarda el color elegido
            auto marcar = [&](int ci, int cj)
            {
                int k = cj * N + ci;
                celdas[k] = valor;
                colores[k * 3 + 0] = colorActual[0];
                colores[k * 3 + 1] = colorActual[1];
                colores[k * 3 + 2] = colorActual[2];
            };

            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            int w, h;
            glfwGetWindowSize(window, &w, &h);

            // Posicion del mouse a celda (la y se invierte porque en la ventana crece hacia abajo)
            int i = (int)(mx / w * N);
            int j = (int)((h - my) / h * N);

            if (i >= 0 && i < N && j >= 0 && j < N)
            {
                if (herramienta == 2)
                {
                    // Relleno: solo actua en el primer instante del clic,
                    // no se repite mientras mantienes el boton presionado
                    if (!dibujando)
                    {
                        rellenar(celdas, colores, N, i, j, colorActual);
                        cambio = true;
                    }
                }
                else if (!dibujando)
                {
                    // Primer clic del trazo: una sola celda
                    marcar(i, j);
                    cambio = true;
                }
                else if (i != ultimaI || j != ultimaJ)
                {
                    // El mouse cambio de celda: Bresenham rellena el camino desde la anterior
                    std::vector<std::pair<int, int>> camino = bresenhamLine(ultimaI, ultimaJ, i, j);
                    for (auto& c : camino)
                    {
                        marcar(c.first, c.second);
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

        // Si algo cambio, se rearman los cuadritos y se suben a la GPU
        if (cambio)
        {
            celdaVertices.clear();

            for (int j = 0; j < N; j++)
            {
                for (int i = 0; i < N; i++)
                {
                    if (celdas[j * N + i] == 0) continue;

                    // Color de esta celda
                    float r = colores[(j * N + i) * 3 + 0];
                    float g = colores[(j * N + i) * 3 + 1];
                    float b = colores[(j * N + i) * 3 + 2];

                    // Esquinas de la celda, con la misma formula de la grilla
                    float x0c = -1.0f + i * step;
                    float y0c = -1.0f + j * step;
                    float x1c = x0c + step;
                    float y1c = y0c + step;

                    // Agrega un vertice: posicion y color de la celda
                    auto agregar = [&](float x, float y)
                    {
                        celdaVertices.push_back(x);
                        celdaVertices.push_back(y);
                        celdaVertices.push_back(r);
                        celdaVertices.push_back(g);
                        celdaVertices.push_back(b);
                    };

                    // Cada cuadrito son dos triangulos
                    agregar(x0c, y0c); agregar(x1c, y0c); agregar(x0c, y1c);
                    agregar(x1c, y0c); agregar(x1c, y1c); agregar(x0c, y1c);
                }
            }

            // DYNAMIC porque estos datos cambian seguido
            glBindBuffer(GL_ARRAY_BUFFER, celdaVBO);
            glBufferData(GL_ARRAY_BUFFER, celdaVertices.size() * sizeof(float), celdaVertices.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        // Dibujo: primero la grilla en blanco, luego las celdas con su color encima
        glUseProgram(shaderProgram);

        // Grilla: color fijo blanco
        glUniform1i(usarVerticeLoc, 0);
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, gridVertices.size() / 2);

        // Celdas: cada una con su color. Se divide entre 5 porque cada vertice tiene 5 numeros
        glUniform1i(usarVerticeLoc, 1);
        glBindVertexArray(celdaVAO);
        glDrawArrays(GL_TRIANGLES, 0, celdaVertices.size() / 5);
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
