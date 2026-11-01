#include <iostream>
#include <vector>  //necesario para std::vector
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Shader de vértices: recibe coordenadas 2D y las pasa a clip space
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos, 0.0, 1.0);\n"
"}\0";

// Shader de fragmentos: recibe el color desde fuera (uniform),
// así podemos usar el mismo shader para pintar la grilla de blanco
// y más adelante las celdas pintadas de otro color, sin crear un shader nuevo.
const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec4 uColor;\n"
"void main()\n"
"{\n"
"   FragColor = uColor;\n"
"}\0";


// Esta función la llama GLFW automáticamente cada vez que cambia el tamaño
// de la ventana (al arrastrar el borde, maximizar o restaurar)
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // glViewport le dice a OpenGL en qué parte de la ventana puede dibujar:
    // desde la esquina inferior izquierda (0, 0) hasta el ancho y alto nuevos.
    // Como la grilla va de -1 a 1, se estira sola a ese espacio
    glViewport(0, 0, width, height);  // la grilla usa toda la ventana
}

int main()
{
    // Inicializar GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Ventana normal de 800x800
    GLFWwindow* window = glfwCreateWindow(800, 800, "Grilla", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Error creando ventana GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Le avisa a GLFW que esa función de arriba es la que debe llamar
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);// cuando cambie el tamaño de la ventana. Solo se registra una vez

    // crea el contexto de ImGui, que guarda todo su estado interno
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // estilo oscuro para los paneles
    ImGui::StyleColorsDark();

    // conecta ImGui con GLFW. El true hace que ImGui reciba el mouse y el teclado
    ImGui_ImplGlfw_InitForOpenGL(window, true);

    // conecta ImGui con OpenGL. La versión debe coincidir con la de tus shaders
    ImGui_ImplOpenGL3_Init("#version 330");

    gladLoadGL();

    // Ajustar viewport al tamaño de la ventana
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

    // Ubicación del uniform uColor, para poder cambiarlo antes de cada dibujo
    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");

    //generar como tal las divisiones y el calculo del tamaño de cada division de la grilla
    const int N = 20; // Numero de cuadros ajustable
    float step = 2.0f / N; //calcula el tamaño del cuadro

    std::vector<float> gridVertices;

    // Líneas verticales
    for (int i = 0; i <= N; i++) {
        float x = -1.0f + i * step;
        gridVertices.push_back(x); gridVertices.push_back(-1.0f);
        gridVertices.push_back(x); gridVertices.push_back(1.0f);
    }
    // Líneas horizontales
    for (int j = 0; j <= N; j++) {
        float y = -1.0f + j * step;
        gridVertices.push_back(-1.0f); gridVertices.push_back(y);
        gridVertices.push_back(1.0f);  gridVertices.push_back(y);
    }

    //grid vao es para la grilla
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

    //condicional para cuando se cierre la ventana
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // avisa a ImGui que empieza un frame nuevo
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // panel de prueba
        ImGui::Begin("Panel");
        ImGui::Text("Hola ImGui");
        ImGui::End();

        glUseProgram(shaderProgram);

        //Dibujar la grilla en blanco
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, gridVertices.size() / 2);

        glBindVertexArray(0);

        // ImGui dibuja al final para que el panel quede encima de la grilla
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    //borrar explicitamente lo que esta en la memoria de la gpu para que se libere y no quede ocupada
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    // libera todo lo que ImGui reservó
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}