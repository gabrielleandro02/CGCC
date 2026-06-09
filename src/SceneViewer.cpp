/* SceneViewer - Visualizador interativo de múltiplos modelos 3D
 *
 * Atividade Vivencial - Módulo 5 | Computação Gráfica - Unisinos
 *
 * Funcionalidades:
 *   - Câmera em primeira pessoa (classe Camera: mover e rotacionar)
 *   - Leitura de arquivos .OBJ (geometria: vértices + normais parseados)
 *   - Leitura de arquivo .MTL (coeficientes Ka, Kd, Ks, Ns para iluminação de Phong)
 *   - Iluminação de Phong por fragmento com 3 luzes pontuais (key, fill, back)
 *   - Atenuação na componente difusa de cada luz
 *   - Posicionamento automático das luzes a partir do objeto principal da cena
 *   - Exibição de múltiplos objetos na cena
 *   - Seleção de objetos com TAB (cicla pela lista)
 *   - Transformações no objeto selecionado:
 *       R  → modo rotação   | X / Y / Z alterna o(s) eixo(s)
 *       T  → modo translação | Setas = XY | Q E = Z
 *       S  → modo escala    | UP ou = aumenta | DOWN ou - diminui
 *   - W/A/S/D   → mover câmera | Mouse → rotacionar câmera
 *   - 1 / 2 / 3 → liga/desliga key / fill / back light
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

const float TRANSLATE_SPEED = 2.0f;
const float SCALE_SPEED     = 0.8f;
const float ROTATE_SPEED    = 1.8f;
const float SCALE_MIN       = 0.05f;

// ---------------------------------------------------------------------------
// Coeficientes de material lidos do .MTL
// ---------------------------------------------------------------------------
struct Material
{
    float ka = 0.2f;
    float kd = 0.7f;
    float ks = 0.5f;
    float q  = 32.0f;
};

// ---------------------------------------------------------------------------
// Struct que representa um modelo 3D na cena
// ---------------------------------------------------------------------------
struct OBJModel
{
    GLuint   VAO       = 0;
    int      nVertices = 0;
    Material mat;

    vec3 position = vec3(0.0f);
    vec3 scale    = vec3(1.0f);
    vec3 rotation = vec3(0.0f);

    string name;
};

// ---------------------------------------------------------------------------
// Modo de transformação ativo
// ---------------------------------------------------------------------------
enum class TransformMode { ROTATE, TRANSLATE, SCALE };

// ---------------------------------------------------------------------------
// Câmera em primeira pessoa
// ---------------------------------------------------------------------------
enum CameraDirection { FORWARD, BACKWARD, LEFT, RIGHT };

class Camera
{
public:
    vec3  position;
    vec3  front;
    vec3  up;
    vec3  right;
    vec3  worldUp;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;

    Camera(vec3 pos = vec3(0.0f, 0.0f, 3.0f),
           float yaw = -90.0f, float pitch = 0.0f)
        : position(pos), worldUp(vec3(0.0f, 1.0f, 0.0f)),
          yaw(yaw), pitch(pitch), speed(5.0f), sensitivity(0.05f)
    {
        updateVectors();
    }

    mat4 getViewMatrix() const
    {
        return lookAt(position, position + front, up);
    }

    void processKeyboard(CameraDirection dir, float dt)
    {
        float v = speed * dt;
        if (dir == FORWARD)  position += front * v;
        if (dir == BACKWARD) position -= front * v;
        if (dir == LEFT)     position -= right * v;
        if (dir == RIGHT)    position += right * v;
    }

    void processMouseMovement(float dx, float dy)
    {
        yaw   += dx * sensitivity;
        pitch += dy * sensitivity;
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateVectors();
    }

private:
    void updateVectors()
    {
        vec3 f;
        f.x   = cos(radians(yaw)) * cos(radians(pitch));
        f.y   = sin(radians(pitch));
        f.z   = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(f);
        right = normalize(cross(front, worldUp));
        up    = normalize(cross(right, front));
    }
};

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------
vector<OBJModel> objects;
int              selectedObj = 0;
TransformMode    mode = TransformMode::ROTATE;

bool rotX = false, rotY = true, rotZ = false;

bool lightEnabled[3] = { true, true, true };

Camera camera;
float  lastX = WIDTH  / 2.0f;
float  lastY = HEIGHT / 2.0f;
bool   firstMouse = true;

// ---------------------------------------------------------------------------
// Protótipos
// ---------------------------------------------------------------------------
void     key_callback(GLFWwindow* window, int key, int scancode, int action, int mod);
void     mouse_callback(GLFWwindow* window, double xpos, double ypos);
int      setupShader();
int      loadSimpleOBJ(const string& filePATH, int& nVertices, string& mtlFile);
Material loadMTL(const string& mtlPath);
void     updateLights(GLuint shaderID);
void     drawObject(GLuint shaderID, const OBJModel& obj, bool selected);
void     printStatus();

// ---------------------------------------------------------------------------
// Vertex Shader
//   location 0 — position (xyz)
//   location 1 — normal   (xyz)
// ---------------------------------------------------------------------------
const GLchar* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vNormal;
out vec3 fragPos;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragPos     = vec3(model * vec4(position, 1.0));
    vNormal     = mat3(transpose(inverse(model))) * normal;
}
)";

// ---------------------------------------------------------------------------
// Fragment Shader — Phong com 3 luzes pontuais e atenuação na difusa
// ---------------------------------------------------------------------------
const GLchar* fragmentShaderSource = R"(
#version 400
in vec3 vNormal;
in vec3 fragPos;

uniform vec3  modelColor;
uniform vec3  camPos;
uniform float ka;
uniform float kd;
uniform float ks;
uniform float q;

uniform vec3  lightPos[3];
uniform vec3  lightColor[3];
uniform int   lightOn[3];

out vec4 color;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(camPos - fragPos);

    vec3 result = ka * modelColor;

    for (int i = 0; i < 3; i++)
    {
        if (lightOn[i] == 0) continue;

        vec3  L    = normalize(lightPos[i] - fragPos);
        float dist = length(lightPos[i] - fragPos);
        float att  = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

        float diff    = max(dot(N, L), 0.0);
        vec3  diffuse = kd * diff * att * lightColor[i] * modelColor;

        vec3  R    = normalize(reflect(-L, N));
        float spec = pow(max(dot(R, V), 0.0), q);
        vec3  specular = ks * spec * lightColor[i];

        result += diffuse + specular;
    }

    color = vec4(result, 1.0);
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
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
    {
        int    nV = 0;
        string mtlFileName;
        int    vao = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nV, mtlFileName);
        if (vao == -1) { glfwTerminate(); return -1; }

        Material mat;
        if (!mtlFileName.empty())
            mat = loadMTL("../assets/Modelos3D/" + mtlFileName);

        OBJModel s1;
        s1.VAO       = (GLuint)vao;
        s1.nVertices = nV;
        s1.mat       = mat;
        s1.position  = vec3(-1.6f, 0.0f, -3.0f);
        s1.name      = "Suzanne #1";
        objects.push_back(s1);

        OBJModel s2;
        s2.VAO       = (GLuint)vao;
        s2.nVertices = nV;
        s2.mat       = mat;
        s2.position  = vec3( 1.6f, 0.0f, -3.0f);
        s2.name      = "Suzanne #2";
        objects.push_back(s2);
    }

    // ---- Uniforms fixos ----
    glUseProgram(shaderID);

    mat4 projection = perspective(radians(50.0f), (float)WIDTH / HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    updateLights(shaderID);

    glEnable(GL_DEPTH_TEST);

    // ---- Ajuda no console ----
    cout << "==================== SceneViewer ====================\n";
    cout << "  W/A/S/D  : mover camera\n";
    cout << "  Mouse    : rotacionar camera\n";
    cout << "  TAB      : selecionar proximo objeto\n";
    cout << "  R        : modo ROTACAO  | X / Y / Z: alternar eixo\n";
    cout << "  T        : modo TRANSLACAO | Setas (XY) | Q/E (Z)\n";
    cout << "  S        : modo ESCALA   | UP ou = : aumenta | DOWN ou - : diminui\n";
    cout << "  1 / 2 / 3: ligar/desligar key / fill / back light\n";
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

        // ---------- Câmera — sempre ativo ----------
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(FORWARD,  dt);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(BACKWARD, dt);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(LEFT,     dt);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(RIGHT,    dt);

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
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) sel.position.x += s;
            if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) sel.position.x -= s;
            if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) sel.position.y += s;
            if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) sel.position.y -= s;
            if (glfwGetKey(window, GLFW_KEY_E)     == GLFW_PRESS) sel.position.z -= s;
            if (glfwGetKey(window, GLFW_KEY_Q)     == GLFW_PRESS) sel.position.z += s;
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

        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE,
                           value_ptr(camera.getViewMatrix()));
        glUniform3f(glGetUniformLocation(shaderID, "camPos"),
                    camera.position.x, camera.position.y, camera.position.z);

        updateLights(shaderID);

        for (int i = 0; i < (int)objects.size(); ++i)
            drawObject(shaderID, objects[i], i == selectedObj);

        glfwSwapBuffers(window);
    }

    // ---- Limpeza ----
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

    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    if (key == GLFW_KEY_TAB)
    {
        selectedObj = (selectedObj + 1) % (int)objects.size();
        cout << "[Selecionado] " << objects[selectedObj].name
             << " (indice " << selectedObj << ")\n";
        return;
    }

    if (key == GLFW_KEY_1) { lightEnabled[0] = !lightEnabled[0]; return; }
    if (key == GLFW_KEY_2) { lightEnabled[1] = !lightEnabled[1]; return; }
    if (key == GLFW_KEY_3) { lightEnabled[2] = !lightEnabled[2]; return; }

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
        mode = TransformMode::SCALE;
        printStatus();
        return;
    }

    if (mode == TransformMode::ROTATE)
    {
        if (key == GLFW_KEY_X) { rotX = !rotX; printStatus(); }
        if (key == GLFW_KEY_Y) { rotY = !rotY; printStatus(); }
        if (key == GLFW_KEY_Z) { rotZ = !rotZ; printStatus(); }
    }
}

// ===========================================================================
// mouse_callback
// ===========================================================================
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float dx =  (float)(xpos - lastX);
    float dy = -(float)(ypos - lastY);
    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.processMouseMovement(dx, dy);
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
// Layout do VBO: pos(3) + normal(3) = 6 floats por vértice
//   location 0 — posição (xyz) @ offset 0
//   location 1 — normal  (xyz) @ offset 3·sizeof(GLfloat)
//
// Retorna o identificador do VAO, ou -1 em caso de erro.
// ===========================================================================
int loadSimpleOBJ(const string& filePATH, int& nVertices, string& mtlFile)
{
    vector<vec3>    positions;
    vector<vec2>    texCoords;
    vector<vec3>    normals;
    vector<GLfloat> vBuffer;

    mtlFile = "";

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

        if (word == "mtllib")
        {
            ss >> mtlFile;
        }
        else if (word == "v")
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

                if (!normals.empty())
                {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(1.0f);
                    vBuffer.push_back(0.0f);
                }
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

    // Atributo 1: normal (3 floats, offset 3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 6);
    return (int)VAO;
}

// ===========================================================================
// loadMTL
//
// Lê coeficientes de iluminação Ka, Kd, Ks e Ns do arquivo .MTL.
// Retorna um Material com defaults caso as linhas não sejam encontradas.
// ===========================================================================
Material loadMTL(const string& mtlPath)
{
    Material mat;

    ifstream file(mtlPath.c_str());
    if (!file.is_open())
    {
        cerr << "Erro ao abrir MTL: " << mtlPath << endl;
        return mat;
    }

    string line;
    while (getline(file, line))
    {
        istringstream ss(line);
        string key;
        ss >> key;

        if (key == "Ka")
        {
            float r, g, b;
            ss >> r >> g >> b;
            mat.ka = (r + g + b) / 3.0f;
        }
        else if (key == "Kd")
        {
            float r, g, b;
            ss >> r >> g >> b;
            mat.kd = (r + g + b) / 3.0f;
        }
        else if (key == "Ks")
        {
            float r, g, b;
            ss >> r >> g >> b;
            mat.ks = (r + g + b) / 3.0f;
        }
        else if (key == "Ns")
        {
            ss >> mat.q;
        }
    }

    file.close();
    cout << "MTL carregado: " << mtlPath
         << "  ka=" << mat.ka << "  kd=" << mat.kd
         << "  ks=" << mat.ks << "  q="  << mat.q << endl;
    return mat;
}

// ===========================================================================
// updateLights
//
// Calcula as posições das 3 luzes em relação ao objeto principal (objects[0])
// e envia os uniforms lightPos, lightColor e lightOn para o shader.
// ===========================================================================
void updateLights(GLuint shaderID)
{
    const OBJModel& main = objects[0];
    float r = glm::max(glm::max(main.scale.x, main.scale.y), main.scale.z) * 4.0f;

    vec3 positions[3] = {
        main.position + vec3(-r,  r * 0.8f,  r),
        main.position + vec3( r,  r * 0.4f,  r),
        main.position + vec3( 0,  r * 0.5f, -r * 1.5f)
    };

    vec3 colors[3] = {
        vec3(1.0f,  1.0f,  0.95f),
        vec3(0.5f,  0.55f, 0.6f),
        vec3(0.6f,  0.6f,  0.65f)
    };

    for (int i = 0; i < 3; i++)
    {
        string posName = "lightPos[" + to_string(i) + "]";
        string colName = "lightColor[" + to_string(i) + "]";
        string onName  = "lightOn[" + to_string(i) + "]";

        glUniform3fv(glGetUniformLocation(shaderID, posName.c_str()), 1, value_ptr(positions[i]));
        glUniform3fv(glGetUniformLocation(shaderID, colName.c_str()), 1, value_ptr(colors[i]));
        glUniform1i (glGetUniformLocation(shaderID, onName.c_str()),  lightEnabled[i] ? 1 : 0);
    }
}

// ===========================================================================
// drawObject
// ===========================================================================
void drawObject(GLuint shaderID, const OBJModel& obj, bool selected)
{
    vec3 color = selected ? vec3(1.0f, 0.55f, 0.0f)
                          : vec3(0.35f, 0.55f, 0.85f);
    glUniform3f(glGetUniformLocation(shaderID, "modelColor"), color.r, color.g, color.b);

    glUniform1f(glGetUniformLocation(shaderID, "ka"), obj.mat.ka);
    glUniform1f(glGetUniformLocation(shaderID, "kd"), obj.mat.kd);
    glUniform1f(glGetUniformLocation(shaderID, "ks"), obj.mat.ks);
    glUniform1f(glGetUniformLocation(shaderID, "q"),  obj.mat.q);

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
// printStatus
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
