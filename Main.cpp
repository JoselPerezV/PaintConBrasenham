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

// Ajusta la celda final de una linea segun la orientacion elegida:
// 0 = libre, 1 = horizontal, 2 = vertical, 3 = diagonal de 45 grados
void ajustarOrientacion(int inicioI, int inicioJ, int& finI, int& finJ, int orientacion)
{
    int dx = finI - inicioI;
    int dy = finJ - inicioJ;

    if (orientacion == 1)
    {
        finJ = inicioJ;
    }
    else if (orientacion == 2)
    {
        finI = inicioI;
    }
    else if (orientacion == 3)
    {
        // Se usa la menor de las dos distancias para que quede justo a 45 grados
        int d = std::min(std::abs(dx), std::abs(dy));
        finI = inicioI + (dx < 0 ? -d : d);
        finJ = inicioJ + (dy < 0 ? -d : d);
    }
}

// Algoritmo del punto medio para circulos, con parametro de decision.
// Recibe el centro (xc, yc) y el radio r, y devuelve las celdas del contorno
std::vector<std::pair<int, int>> bresenhamCircle(int xc, int yc, int r)
{
    std::vector<std::pair<int, int>> puntos;

    // Solo se calcula un octavo del circulo, los otros siete salen por simetria
    int x = 0;
    int y = r;
    int p = 1 - r; // parametro de decision inicial

    // Dado un punto (x, y) del primer octavo, agrega sus 8 puntos simetricos
    auto plotPoints = [&](int x, int y)
    {
        puntos.push_back({ xc + x, yc + y });
        puntos.push_back({ xc - x, yc + y });
        puntos.push_back({ xc + x, yc - y });
        puntos.push_back({ xc - x, yc - y });
        puntos.push_back({ xc + y, yc + x });
        puntos.push_back({ xc - y, yc + x });
        puntos.push_back({ xc + y, yc - x });
        puntos.push_back({ xc - y, yc - x });
    };

    plotPoints(x, y);

    while (x < y)
    {
        x++;
        if (p < 0)
        {
            p += 2 * x + 1;
        }
        else
        {
            y--;
            p += 2 * (x - y) + 1;
        }
        plotPoints(x, y);
    }

    return puntos;
}

// Calcula las celdas de una figura entre la celda donde empezo el clic y la del mouse.
// 3 = linea, 4 = circulo, 5 = cuadrado, 6 = triangulo
std::vector<std::pair<int, int>> calcularFigura(int herramienta, int inicioI, int inicioJ, int mouseI, int mouseJ, int orientacion, int N)
{
    std::vector<std::pair<int, int>> figura;

    int dx = mouseI - inicioI;
    int dy = mouseJ - inicioJ;

    // Agrega a la figura las celdas de una recta entre dos celdas
    auto agregarRecta = [&](int ax, int ay, int bx, int by)
    {
        std::vector<std::pair<int, int>> recta = bresenhamLine(ax, ay, bx, by);
        figura.insert(figura.end(), recta.begin(), recta.end());
    };

    if (herramienta == 3)
    {
        // Linea: la celda final se ajusta segun la orientacion elegida
        int finI = mouseI;
        int finJ = mouseJ;
        ajustarOrientacion(inicioI, inicioJ, finI, finJ, orientacion);
        agregarRecta(inicioI, inicioJ, finI, finJ);
    }
    else if (herramienta == 4)
    {
        // Circulo: el centro es el inicio y el radio es la distancia hasta el mouse
        int r = (int)std::lround(std::sqrt((double)(dx * dx + dy * dy)));
        figura = bresenhamCircle(inicioI, inicioJ, r);
    }
    else if (herramienta == 5)
    {
        // Cuadrado: se usa el lado menor para que quede cuadrado y dentro de la grilla
        int lado = std::min(std::abs(dx), std::abs(dy));
        int x1 = inicioI + (dx < 0 ? -lado : lado);
        int y1 = inicioJ + (dy < 0 ? -lado : lado);

        agregarRecta(inicioI, inicioJ, x1, inicioJ);  // lado de abajo
        agregarRecta(x1, inicioJ, x1, y1);            // lado derecho
        agregarRecta(x1, y1, inicioI, y1);            // lado de arriba
        agregarRecta(inicioI, y1, inicioI, inicioJ);  // lado izquierdo
    }
    else if (herramienta == 6)
    {
        // Triangulo: base en la fila del clic y punta en la fila del mouse, en el centro
        int xm = (inicioI + mouseI) / 2;

        agregarRecta(inicioI, inicioJ, mouseI, inicioJ);  // base
        agregarRecta(inicioI, inicioJ, xm, mouseJ);       // lado izquierdo
        agregarRecta(mouseI, inicioJ, xm, mouseJ);        // lado derecho
    }

    // Se descartan las celdas fuera de la grilla (el circulo se puede salir)
    std::vector<std::pair<int, int>> dentro;
    for (auto& c : figura)
    {
        if (c.first >= 0 && c.first < N && c.second >= 0 && c.second < N)
            dentro.push_back(c);
    }
    return dentro;
}


// Aplica mover, girar y escalar a todo lo pintado.
// Girar y escalar se hacen alrededor del centro del dibujo
void transformarDibujo(std::vector<int>& celdas, std::vector<float>& colores, int N, int tx, int ty, float anguloGrados, float escala)
{
    // Caja que rodea al dibujo: la celda pintada mas a la izquierda, derecha, abajo y arriba
    int minI = N, maxI = -1, minJ = N, maxJ = -1;
    for (int j = 0; j < N; j++)
    {
        for (int i = 0; i < N; i++)
        {
            if (celdas[j * N + i] == 1)
            {
                minI = std::min(minI, i);
                maxI = std::max(maxI, i);
                minJ = std::min(minJ, j);
                maxJ = std::max(maxJ, j);
            }
        }
    }

    // Si no hay nada pintado, no hay nada que transformar
    if (maxI < 0) return;

    // Centro del dibujo
    float cx = (minI + maxI + 1) / 2.0f;
    float cy = (minJ + maxJ + 1) / 2.0f;

    float a = anguloGrados * 3.14159265f / 180.0f;
    float c = std::cos(a);
    float s = std::sin(a);

    std::vector<int> nuevasCeldas(N * N, 0);
    std::vector<float> nuevosColores(N * N * 3, 0.0f);

    // Se recorre cada celda del resultado y se busca de donde viene.
    // Asi no quedan huecos al girar o agrandar
    for (int j = 0; j < N; j++)
    {
        for (int i = 0; i < N; i++)
        {
            // Centro de la celda de destino, medido desde el centro del dibujo y sin el movimiento
            float px = i + 0.5f - cx - tx;
            float py = j + 0.5f - cy - ty;

            // Se deshace el giro y la escala para encontrar la celda de origen
            float ox = (c * px + s * py) / escala + cx;
            float oy = (-s * px + c * py) / escala + cy;

            int oi = (int)std::floor(ox + 0.0001f);
            int oj = (int)std::floor(oy + 0.0001f);

            // Si el origen cae fuera de la grilla, la celda queda vacia
            if (oi < 0 || oi >= N || oj < 0 || oj >= N) continue;

            int origen = oj * N + oi;
            int destino = j * N + i;

            // Si la celda de origen estaba pintada, la de destino se pinta del mismo color
            if (celdas[origen] == 1)
            {
                nuevasCeldas[destino] = 1;
                nuevosColores[destino * 3 + 0] = colores[origen * 3 + 0];
                nuevosColores[destino * 3 + 1] = colores[origen * 3 + 1];
                nuevosColores[destino * 3 + 2] = colores[origen * 3 + 2];
            }
        }
    }

    celdas = nuevasCeldas;
    colores = nuevosColores;
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

    // Color con el que se pinta ahora (el que se elige en el panel)
    float colorActual[3] = { 0.3f, 0.5f, 1.0f };

    // Herramienta activa: 0 = pincel, 1 = borrador, 2 = relleno, 3 = linea,
    // 4 = circulo, 5 = cuadrado, 6 = triangulo
    int herramienta = 0;

    // Orientacion de la linea: 0 = libre, 1 = horizontal, 2 = vertical, 3 = diagonal
    int orientacion = 0;

    // Celdas de la vista previa de la figura (se marcan mientras arrastras)
    std::vector<int> vistaPrevia(N * N, 0);

    // Celda donde empezo la figura y si hay una figura en curso
    int inicioI = -1, inicioJ = -1;
    bool hayLinea = false;

    //Valores de los controles de transformacion
    int tx = 0, ty = 0;
    float angulo = 0.0f;
    float escala = 1.0f;

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

        // Panel de controles: se ajusta solo al tamano de lo que tiene adentro
        ImGui::Begin("Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        // Selector de color: al elegir uno, las celdas nuevas se pintan con ese color
        ImGui::ColorEdit3("Color", colorActual);

        // Botones para elegir la herramienta
        ImGui::RadioButton("Pincel", &herramienta, 0);
        ImGui::RadioButton("Borrador", &herramienta, 1);
        ImGui::RadioButton("Relleno", &herramienta, 2);
        ImGui::RadioButton("Linea", &herramienta, 3);
        ImGui::RadioButton("Circulo", &herramienta, 4);
        ImGui::RadioButton("Cuadrado", &herramienta, 5);
        ImGui::RadioButton("Triangulo", &herramienta, 6);

        // El selector de orientacion solo aparece con la herramienta de linea
        if (herramienta == 3)
        {
            const char* orientaciones[] = { "Libre", "Horizontal", "Vertical", "Diagonal 45" };
            ImGui::Combo("Orientacion", &orientacion, orientaciones, 4);
        }

        // Si se presiona, todas las celdas quedan vacias
        if (ImGui::Button("Borrar todo"))
        {
            std::fill(celdas.begin(), celdas.end(), 0);
            cambio = true;
        }

        ImGui::Separator();
        ImGui::Text("Transformaciones");

        // Controles: mover en x y en y, girar en grados y escalar
        ImGui::SliderInt("Mover X", &tx, -N, N);
        ImGui::SliderInt("Mover Y", &ty, -N, N);
        ImGui::SliderFloat("Angulo", &angulo, -180.0f, 180.0f);
        ImGui::SliderFloat("Escala", &escala, 0.25f, 3.0f);

        // Aplica los valores al dibujo y deja los controles en su valor inicial
        if (ImGui::Button("Aplicar"))
        {
            transformarDibujo(celdas, colores, N, tx, ty, angulo, escala);
            tx = 0; ty = 0; angulo = 0.0f; escala = 1.0f;
            cambio = true;
        }

        // Deja los controles en su valor inicial sin tocar el dibujo
        ImGui::SameLine();
        if (ImGui::Button("Restablecer"))
        {
            tx = 0; ty = 0; angulo = 0.0f; escala = 1.0f;
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
                if (herramienta >= 3)
                {
                    // Linea, circulo, cuadrado y triangulo: el primer instante del clic fija el inicio
                    if (!dibujando)
                    {
                        inicioI = i;
                        inicioJ = j;
                    }

                    // Vista previa: se borra la anterior y se marca la figura nueva
                    std::fill(vistaPrevia.begin(), vistaPrevia.end(), 0);
                    std::vector<std::pair<int, int>> figura = calcularFigura(herramienta, inicioI, inicioJ, i, j, orientacion, N);
                    for (auto& c : figura)
                    {
                        vistaPrevia[c.second * N + c.first] = 1;
                    }

                    hayLinea = true;
                    cambio = true;
                }
                else if (herramienta == 2)
                {
                    // Relleno: solo actua en el primer instante del clic
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
            // Si habia una figura en curso, al soltar el clic se pinta de verdad
            if (dibujando && herramienta >= 3 && hayLinea)
            {
                for (int k = 0; k < N * N; k++)
                {
                    if (vistaPrevia[k] == 1)
                    {
                        celdas[k] = 1;
                        colores[k * 3 + 0] = colorActual[0];
                        colores[k * 3 + 1] = colorActual[1];
                        colores[k * 3 + 2] = colorActual[2];
                    }
                }

                // La vista previa ya cumplio su trabajo
                std::fill(vistaPrevia.begin(), vistaPrevia.end(), 0);
                hayLinea = false;
                cambio = true;
            }

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
                    int k = j * N + i;

                    // Una celda se dibuja si esta pintada o si es parte de la vista previa
                    bool enVista = (vistaPrevia[k] == 1);
                    if (celdas[k] == 0 && !enVista) continue;

                    // La vista previa usa el color elegido, las demas su propio color
                    float r = enVista ? colorActual[0] : colores[k * 3 + 0];
                    float g = enVista ? colorActual[1] : colores[k * 3 + 1];
                    float b = enVista ? colorActual[2] : colores[k * 3 + 2];

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