/* SceneViewer - Visualizador interativo de múltiplos modelos 3D
 *
 * Atividade Vivencial - Módulo 2 | Computação Gráfica - Unisinos
 *
 * Funcionalidades:
 *   - Leitura de arquivos .OBJ (geometria: vértices + normais + texcoords parseados)
 *   - Exibição de múltiplos objetos na cena
 *   - Seleção de objetos com TAB (cicla pela lista)
 *   - Transformações no objeto selecionado:
 *       R  → modo rotação   | X / Y / Z alterna o(s) eixo(s)
 *       T  → modo translação | W A D / Setas = XY | Q E = Z
 *       S  → modo escala    | UP ou = aumenta | DOWN ou - diminui
 *   - ESC → fechar janela
 *
 * Objeto selecionado: laranja | Objetos não-selecionados: azul-aço
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;

// ---------------------------------------------------------------------------
// Constantes
// ---------------------------------------------------------------------------
const GLuint WIDTH  = 900;
const GLuint HEIGHT = 700;

const float TRANSLATE_SPEED = 2.0f;   // unidades / segundo
const float SCALE_SPEED     = 0.8f;   // fator   / segundo
const float ROTATE_SPEED    = 1.8f;   // radianos / segundo
const float SCALE_MIN       = 0.05f;

// ---------------------------------------------------------------------------
// Struct que representa um modelo 3D na cena
// ---------------------------------------------------------------------------
struct OBJModel
{
    GLuint VAO       = 0;
    int    nVertices = 0;

    vec3 position = vec3(0.0f);       // translação no mundo
    vec3 scale    = vec3(1.0f);       // escala nos três eixos
    vec3 rotation = vec3(0.0f);       // ângulos de Euler em radianos (XYZ)

    string name;                      // apenas para log
};

// ---------------------------------------------------------------------------
// Modo de transformação ativo
// ---------------------------------------------------------------------------
enum class TransformMode { ROTATE, TRANSLATE, SCALE };

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------
vector<OBJModel> objects;
int              selectedObj = 0;
TransformMode    mode = TransformMode::ROTATE;

bool rotX = false, rotY = true, rotZ = false;  // eixos de rotação ativos

// ---------------------------------------------------------------------------
// Protótipos
// ---------------------------------------------------------------------------
void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mod);
int    setupShader();
int    loadSimpleOBJ(const string& filePATH, int& nVertices);
void   drawObject(GLuint shaderID, const OBJModel& obj, bool selected);
void   printStatus();

// ---------------------------------------------------------------------------
// Vertex Shader
//   location 0 — position (xyz)
//   location 1 — color    (rgb)  — mantido para compatibilidade com o VBO;
//                                   a cor efetiva vem do uniform modelColor
// ---------------------------------------------------------------------------
const GLchar* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

uniform mat4 model;
uniform mat4 projection;
uniform vec3 modelColor;

out vec4 vertexColor;

void main()
{
    gl_Position = projection * model * vec4(position, 1.0);
    vertexColor = vec4(modelColor, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Fragment Shader
// ---------------------------------------------------------------------------
const GLchar* fragmentShaderSource = R"(
#version 400
in  vec4 vertexColor;
out vec4 color;

void main()
{
    color = vertexColor;
}
)";

// ===========================================================================
// MAIN
// ===========================================================================
int main()
{
    // ---- GLFW ----
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
                                          "SceneViewer - Selecione e Transforme", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    // ---- GLAD ----
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version: " << version << "\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    // ---- Shaders ----
    GLuint shaderID = setupShader();

    // ---- Carregar modelos ----
    // Os dois modelos compartilham o mesmo VAO (mesma geometria, transforms diferentes)
    {
        int nV = 0;
        int vao = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nV);
        if (vao == -1) { glfwTerminate(); return -1; }

        OBJModel s1;
        s1.VAO       = (GLuint)vao;
        s1.nVertices = nV;
        s1.position  = vec3(-1.6f, 0.0f, -5.0f);
        s1.name      = "Suzanne #1";
        objects.push_back(s1);

        OBJModel s2;
        s2.VAO       = (GLuint)vao;   // compartilha geometria
        s2.nVertices = nV;
        s2.position  = vec3( 1.6f, 0.0f, -5.0f);
        s2.name      = "Suzanne #2";
        objects.push_back(s2);
    }

    // ---- Projeção perspectiva ----
    glUseProgram(shaderID);
    mat4 projection = perspective(radians(50.0f), (float)WIDTH / HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    glEnable(GL_DEPTH_TEST);

    // ---- Ajuda no console ----
    cout << "==================== SceneViewer ====================\n";
    cout << "  TAB      : selecionar proximo objeto\n";
    cout << "  R        : modo ROTACAO  | X / Y / Z: alternar eixo\n";
    cout << "  T        : modo TRANSLACAO | W/A/D ou Setas (XY) | Q/E (Z)\n";
    cout << "  S        : modo ESCALA   | UP ou = : aumenta | DOWN ou - : diminui\n";
    cout << "  ESC      : fechar\n";
    cout << "=====================================================\n\n";
    printStatus();

    // ---- Delta time ----
    float lastTime = (float)glfwGetTime();

    // ---- Game loop ----
    while (!glfwWindowShouldClose(window))
    {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        glfwPollEvents();

        OBJModel& sel = objects[selectedObj];

        // ---------- Transformações contínuas (glfwGetKey) ----------

        if (mode == TransformMode::ROTATE)
        {
            if (rotX) sel.rotation.x += ROTATE_SPEED * dt;
            if (rotY) sel.rotation.y += ROTATE_SPEED * dt;
            if (rotZ) sel.rotation.z += ROTATE_SPEED * dt;
        }
        else if (mode == TransformMode::TRANSLATE)
        {
            float s = TRANSLATE_SPEED * dt;
            // Eixo X
            if (glfwGetKey(window, GLFW_KEY_D)     == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT)  == GLFW_PRESS)
                sel.position.x += s;
            if (glfwGetKey(window, GLFW_KEY_A)     == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_LEFT)   == GLFW_PRESS)
                sel.position.x -= s;
            // Eixo Y
            if (glfwGetKey(window, GLFW_KEY_W)     == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_UP)     == GLFW_PRESS)
                sel.position.y += s;
            if (glfwGetKey(window, GLFW_KEY_S)     == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_DOWN)   == GLFW_PRESS)
                sel.position.y -= s;
            // Eixo Z
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                sel.position.z -= s;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                sel.position.z += s;
        }
        else if (mode == TransformMode::SCALE)
        {
            float s = SCALE_SPEED * dt;
            if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_EQUAL)  == GLFW_PRESS)
                sel.scale = max(sel.scale + vec3(s), vec3(SCALE_MIN));
            if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_MINUS)  == GLFW_PRESS)
                sel.scale = max(sel.scale - vec3(s), vec3(SCALE_MIN));
        }

        // ---------- Renderização ----------
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderID);

        for (int i = 0; i < (int)objects.size(); ++i)
            drawObject(shaderID, objects[i], i == selectedObj);

        glfwSwapBuffers(window);
    }

    // ---- Limpeza ----
    // Coleta VAOs únicos para não deletar o mesmo duas vezes
    GLuint lastVAO = 0;
    for (auto& obj : objects)
    {
        if (obj.VAO != lastVAO)
        {
            glDeleteVertexArrays(1, &obj.VAO);
            lastVAO = obj.VAO;
        }
    }
    glfwTerminate();
    return 0;
}

// ===========================================================================
// key_callback
// ===========================================================================
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    if (action != GLFW_PRESS) return;

    // ---- Fechar ----
    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    // ---- Seleção de objeto ----
    if (key == GLFW_KEY_TAB)
    {
        selectedObj = (selectedObj + 1) % (int)objects.size();
        cout << "[Selecionado] " << objects[selectedObj].name
             << " (indice " << selectedObj << ")\n";
        return;
    }

    // ---- Troca de modo ----
    if (key == GLFW_KEY_R)
    {
        mode = TransformMode::ROTATE;
        printStatus();
        return;
    }
    if (key == GLFW_KEY_T)
    {
        mode = TransformMode::TRANSLATE;
        printStatus();
        return;
    }
    if (key == GLFW_KEY_S && mode != TransformMode::TRANSLATE)
    {
        // S muda para SCALE apenas quando não está em TRANSLATE
        // (em TRANSLATE, S é usado como tecla de movimento no game loop)
        mode = TransformMode::SCALE;
        printStatus();
        return;
    }

    // ---- Eixos de rotação (apenas no modo ROTATE) ----
    if (mode == TransformMode::ROTATE)
    {
        if (key == GLFW_KEY_X) { rotX = !rotX; printStatus(); }
        if (key == GLFW_KEY_Y) { rotY = !rotY; printStatus(); }
        if (key == GLFW_KEY_Z) { rotZ = !rotZ; printStatus(); }
    }
}

// ===========================================================================
// setupShader
// ===========================================================================
int setupShader()
{
    GLint  success;
    GLchar infoLog[512];

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vs, 512, NULL, infoLog);
        cerr << "VERTEX SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs, 512, NULL, infoLog);
        cerr << "FRAGMENT SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(prog, 512, NULL, infoLog);
        cerr << "SHADER LINK ERROR:\n" << infoLog << endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return (int)prog;
}

// ===========================================================================
// loadSimpleOBJ
//
// Layout do VBO: pos(3) + cor_placeholder(3) = 6 floats por vértice
//   location 0 — posição (xyz) @ offset 0
//   location 1 — cor     (rgb) @ offset 3·sizeof(GLfloat)
//
// A cor efetiva é controlada pelo uniform modelColor no shader.
// Retorna o identificador do VAO, ou -1 em caso de erro.
// ===========================================================================
int loadSimpleOBJ(const string& filePATH, int& nVertices)
{
    vector<vec3>    positions;
    vector<vec2>    texCoords;   // lidos mas não inseridos no buffer neste modo
    vector<vec3>    normals;     // lidos mas não inseridos no buffer neste modo
    vector<GLfloat> vBuffer;

    const vec3 placeholderColor(1.0f, 1.0f, 1.0f); // branco; cor real vem do uniform

    ifstream arq(filePATH.c_str());
    if (!arq.is_open())
    {
        cerr << "Erro ao abrir: " << filePATH << endl;
        return -1;
    }

    string line;
    while (getline(arq, line))
    {
        istringstream ss(line);
        string word;
        ss >> word;

        if (word == "v")
        {
            vec3 v;
            ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (word == "vt")
        {
            vec2 vt;
            ss >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            vec3 vn;
            ss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (word == "f")
        {
            // Suporte a faces triangulares: v/vt/vn  v//vn  v/vt  v
            while (ss >> word)
            {
                int vi = 0, ti = 0, ni = 0;
                istringstream fs(word);
                string idx;

                if (getline(fs, idx, '/')) vi = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(fs, idx, '/')) ti = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(fs, idx))      ni = !idx.empty() ? stoi(idx) - 1 : 0;

                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);
                vBuffer.push_back(placeholderColor.r);
                vBuffer.push_back(placeholderColor.g);
                vBuffer.push_back(placeholderColor.b);
            }
        }
    }
    arq.close();

    cout << "OBJ carregado: " << filePATH
         << "  |  vertices=" << positions.size()
         << "  texcoords=" << texCoords.size()
         << "  normais=" << normals.size() << endl;

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Atributo 0: posição (3 floats, offset 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: cor placeholder (3 floats, offset 3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // pos(3) + cor(3) = 6 floats por vértice
    nVertices = (int)(vBuffer.size() / 6);
    return (int)VAO;
}

// ===========================================================================
// drawObject
// Constrói a matriz de modelo (T · Rx · Ry · Rz · S), envia os uniforms
// e executa o drawcall.
// ===========================================================================
void drawObject(GLuint shaderID, const OBJModel& obj, bool selected)
{
    // Cor: laranja para selecionado, azul-aço para os demais
    vec3 color = selected ? vec3(1.0f, 0.55f, 0.0f)
                          : vec3(0.35f, 0.55f, 0.85f);
    glUniform3f(glGetUniformLocation(shaderID, "modelColor"), color.r, color.g, color.b);

    // Matriz de modelo: Translate · RotX · RotY · RotZ · Scale
    mat4 model = mat4(1.0f);
    model = translate(model, obj.position);
    model = rotate(model, obj.rotation.x, vec3(1.0f, 0.0f, 0.0f));
    model = rotate(model, obj.rotation.y, vec3(0.0f, 1.0f, 0.0f));
    model = rotate(model, obj.rotation.z, vec3(0.0f, 0.0f, 1.0f));
    model = scale(model, obj.scale);

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

    glBindVertexArray(obj.VAO);
    glDrawArrays(GL_TRIANGLES, 0, obj.nVertices);
    glBindVertexArray(0);
}

// ===========================================================================
// printStatus — exibe no console o estado atual
// ===========================================================================
void printStatus()
{
    const char* modeStr =
        mode == TransformMode::ROTATE    ? "ROTACAO" :
        mode == TransformMode::TRANSLATE ? "TRANSLACAO" : "ESCALA";

    cout << "[Modo: " << modeStr << "] "
         << "[Objeto: " << objects[selectedObj].name << "]";

    if (mode == TransformMode::ROTATE)
        cout << "  Eixos: "
             << (rotX ? "X " : "")
             << (rotY ? "Y " : "")
             << (rotZ ? "Z " : "")
             << (!rotX && !rotY && !rotZ ? "(nenhum)" : "");

    cout << "\n";
}
