/* SceneViewer - Visualizador interativo de múltiplos modelos 3D
 *
 * Grau B | Computação Gráfica - Unisinos
 *
 * Funcionalidades:
 *   - Cena definida por arquivo de configuração (assets/cena.txt)
 *   - Câmera em primeira pessoa (classe Camera: mover e rotacionar)
 *   - Leitura de arquivos .OBJ (geometria: vértices + normais + UVs)
 *   - Leitura de arquivo .MTL (coeficientes Ka, Kd, Ks, Ns para Phong)
 *   - Mapeamento de textura por objeto (toggle com U)
 *   - Iluminação de Phong por fragmento com 3 luzes pontuais (key, fill, back)
 *   - Atenuação na componente difusa de cada luz
 *   - Seleção de objetos com TAB
 *   - Transformações: R (rotação X/Y/Z) | T (translação setas/Q/E) | S (escala)
 *   - W/A/S/D + Mouse → câmera | 1/2/3 → luzes | P/F/G → trajetória Bézier
 *   - U → alternar textura / cor sólida | ESC → fechar
 *
 * Objeto selecionado: laranja | Objetos não-selecionados: azul-aço
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

const float TRANSLATE_SPEED  = 2.0f;
const float SCALE_SPEED      = 0.8f;
const float ROTATE_SPEED     = 1.8f;
const float SCALE_MIN        = 0.05f;
const float TRAJECTORY_SPEED = 2.0f;

// ---------------------------------------------------------------------------
// Coeficientes de material lidos do .MTL
// ---------------------------------------------------------------------------
struct Material
{
    float ka = 0.2f;
    float kd = 0.7f;
    float ks = 0.5f;
    float q  = 32.0f;
    vec3  Kd = vec3(0.64f);
};

// ---------------------------------------------------------------------------
// Struct que representa um modelo 3D na cena
// ---------------------------------------------------------------------------
struct OBJModel
{
    GLuint   VAO       = 0;
    int      nVertices = 0;
    Material mat;
    GLuint   textureID = 0;

    vec3 position = vec3(0.0f);
    vec3 scale    = vec3(1.0f);
    vec3 rotation = vec3(0.0f);

    string       name;
    vector<vec3> waypoints;
    int          waypointIdx = 0;
    float        waypointT   = 0.0f;
    bool         following   = false;
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
TransformMode    mode = TransformMode::TRANSLATE;

bool rotX = false, rotY = false, rotZ = false;

vec3 lightPositions[3] = {
    vec3(-4.0f,  4.0f,  4.0f),
    vec3( 4.0f,  2.0f,  4.0f),
    vec3( 0.0f,  2.5f, -6.0f)
};
vec3 lightColors[3] = {
    vec3(1.0f,  1.0f,  0.95f),
    vec3(0.5f,  0.55f, 0.6f),
    vec3(0.6f,  0.6f,  0.65f)
};
bool lightEnabled[3] = { true, true, true };

bool globalUseTexture = true;

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
GLuint   loadTexture(const string& path);
void     loadScene(const string& path, GLuint shaderID);
void     updateLights(GLuint shaderID);
void     drawObject(GLuint shaderID, const OBJModel& obj, bool selected);
void     printStatus();

// ---------------------------------------------------------------------------
// Vertex Shader
//   location 0 — position  (xyz)
//   location 1 — normal    (xyz)
//   location 2 — texcoord  (uv)
// ---------------------------------------------------------------------------
const GLchar* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vNormal;
out vec3 fragPos;
out vec2 vTexCoord;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragPos     = vec3(model * vec4(position, 1.0));
    vNormal     = mat3(transpose(inverse(model))) * normal;
    vTexCoord   = texCoord;
}
)";

// ---------------------------------------------------------------------------
// Fragment Shader — Phong com 3 luzes pontuais, atenuação e textura
// ---------------------------------------------------------------------------
const GLchar* fragmentShaderSource = R"(
#version 400
in vec3 vNormal;
in vec3 fragPos;
in vec2 vTexCoord;

uniform vec3  modelColor;
uniform vec3  camPos;
uniform float ka;
uniform float kd;
uniform float ks;
uniform float q;

uniform vec3  lightPos[3];
uniform vec3  lightColor[3];
uniform int   lightOn[3];

uniform sampler2D texSampler;
uniform int       useTexture;

out vec4 color;

void main()
{
    vec3 baseColor = (useTexture == 1) ? texture(texSampler, vTexCoord).rgb : modelColor;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(camPos - fragPos);

    vec3 result = ka * baseColor;

    for (int i = 0; i < 3; i++)
    {
        if (lightOn[i] == 0) continue;

        vec3  L    = normalize(lightPos[i] - fragPos);
        float dist = length(lightPos[i] - fragPos);
        float att  = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

        float diff    = max(dot(N, L), 0.0);
        vec3  diffuse = kd * diff * att * lightColor[i] * baseColor;

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
int main(int argc, char* argv[])
{
    // ---- GLFW ----
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
                                          "SceneViewer - Grau B", nullptr, nullptr);
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
    glUseProgram(shaderID);

    // ---- Carregar cena ----
    string sceneFile = (argc > 1) ? argv[1] : "../assets/cena.txt";
    loadScene(sceneFile, shaderID);

    if (objects.empty())
    {
        cerr << "Nenhum objeto carregado. Verifique assets/cena.txt." << endl;
        glfwTerminate();
        return -1;
    }

    // ---- Uniforms fixos ----
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
    cout << "  P        : adicionar ponto de controle Bezier\n";
    cout << "  F        : ligar/desligar trajetoria (min 4 pontos)\n";
    cout << "  G        : limpar trajetoria\n";
    cout << "  U        : alternar textura / cor solida\n";
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

        // ---------- Título da janela com comandos ----------
        if (!objects.empty())
        {
            const string& obj = objects[selectedObj].name;
            string hint;
            if (mode == TransformMode::TRANSLATE)
                hint = "E:Avanca  Q:Recua  Setas:Lateral/Altura  |  TAB:Obj  R:Rotacao  S:Escala  P:Waypoint  F:Seguir  U:Textura";
            else if (mode == TransformMode::ROTATE)
                hint = "X/Y/Z:Eixo  Setas:Girar  |  TAB:Obj  T:Translacao  S:Escala  P:Waypoint  F:Seguir  U:Textura";
            else
                hint = "Setas/+-:Escalar  |  TAB:Obj  T:Translacao  R:Rotacao  P:Waypoint  F:Seguir  U:Textura";
            string title = "SceneViewer | " + obj + " | " + hint;
            glfwSetWindowTitle(window, title.c_str());
        }

        // ---------- Câmera — sempre ativo ----------
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(FORWARD,  dt);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(BACKWARD, dt);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(LEFT,     dt);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(RIGHT,    dt);

        // ---------- Trajetórias Bézier cúbico ----------
        for (auto& obj : objects)
        {
            if (!obj.following || (int)obj.waypoints.size() < 4) continue;

            int numSegs = ((int)obj.waypoints.size() - 1) / 3;
            int base    = obj.waypointIdx * 3;
            vec3 P0 = obj.waypoints[base];
            vec3 P1 = obj.waypoints[base + 1];
            vec3 P2 = obj.waypoints[base + 2];
            vec3 P3 = obj.waypoints[base + 3];

            float dist = length(P3 - P0);
            if (dist > 0.001f)
                obj.waypointT += TRAJECTORY_SPEED * dt / dist;

            if (obj.waypointT >= 1.0f)
            {
                obj.waypointT   = 0.0f;
                obj.waypointIdx = (obj.waypointIdx + 1) % numSegs;
            }

            float t = obj.waypointT;
            float u = 1.0f - t;
            obj.position = u*u*u*P0 + 3.0f*u*u*t*P1 + 3.0f*u*t*t*P2 + t*t*t*P3;
        }

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
        if (obj.textureID != 0)
            glDeleteTextures(1, &obj.textureID);
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

    if (key == GLFW_KEY_U)
    {
        globalUseTexture = !globalUseTexture;
        cout << "[Textura] " << (globalUseTexture ? "ATIVA\n" : "DESATIVADA\n");
        return;
    }

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

    if (key == GLFW_KEY_P)
    {
        objects[selectedObj].waypoints.push_back(objects[selectedObj].position);
        cout << "[Waypoint] " << objects[selectedObj].name
             << "  total=" << objects[selectedObj].waypoints.size() << "\n";
        return;
    }
    if (key == GLFW_KEY_F)
    {
        OBJModel& o = objects[selectedObj];
        if ((int)o.waypoints.size() >= 4)
        {
            o.following   = !o.following;
            o.waypointIdx = 0;
            o.waypointT   = 0.0f;
            cout << "[Trajetoria] " << o.name
                 << (o.following ? " LIGADA\n" : " DESLIGADA\n");
        }
        else
        {
            cout << "[Trajetoria] Necessario ao menos 4 pontos de controle\n";
        }
        return;
    }
    if (key == GLFW_KEY_G)
    {
        OBJModel& o = objects[selectedObj];
        o.waypoints.clear();
        o.following   = false;
        o.waypointIdx = 0;
        o.waypointT   = 0.0f;
        cout << "[Trajetoria limpa] " << o.name << "\n";
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
// Layout do VBO: pos(3) + normal(3) + texcoord(2) = 8 floats por vértice
//   location 0 — posição  (xyz) @ offset 0
//   location 1 — normal   (xyz) @ offset 3·sizeof(GLfloat)
//   location 2 — texcoord (uv)  @ offset 6·sizeof(GLfloat)
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
            struct FV { int vi, ti, ni; };
            vector<FV> face;

            while (ss >> word)
            {
                FV fv = {0, 0, 0};
                istringstream fs(word);
                string idx;
                if (getline(fs, idx, '/')) fv.vi = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(fs, idx, '/')) fv.ti = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(fs, idx))      fv.ni = !idx.empty() ? stoi(idx) - 1 : 0;
                face.push_back(fv);
            }

            auto pushFV = [&](const FV& fv)
            {
                vBuffer.push_back(positions[fv.vi].x);
                vBuffer.push_back(positions[fv.vi].y);
                vBuffer.push_back(positions[fv.vi].z);
                if (!normals.empty())
                {
                    vBuffer.push_back(normals[fv.ni].x);
                    vBuffer.push_back(normals[fv.ni].y);
                    vBuffer.push_back(normals[fv.ni].z);
                }
                else { vBuffer.push_back(0.0f); vBuffer.push_back(1.0f); vBuffer.push_back(0.0f); }
                if (!texCoords.empty())
                {
                    vBuffer.push_back(texCoords[fv.ti].s);
                    vBuffer.push_back(texCoords[fv.ti].t);
                }
                else { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }
            };

            for (int i = 1; i + 1 < (int)face.size(); i++)
            {
                pushFV(face[0]);
                pushFV(face[i]);
                pushFV(face[i + 1]);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: normal (3 floats, offset 3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Atributo 2: texcoord (2 floats, offset 6)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 8);
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
            float sat    = max({r, g, b}) - min({r, g, b});
            float curSat = max({mat.Kd.r, mat.Kd.g, mat.Kd.b})
                         - min({mat.Kd.r, mat.Kd.g, mat.Kd.b});
            if (sat >= curSat)
            {
                mat.kd = (r + g + b) / 3.0f;
                mat.Kd = vec3(r, g, b);
            }
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
// loadTexture
// ===========================================================================
GLuint loadTexture(const string& path)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (data)
    {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        cout << "Textura carregada: " << path
             << "  " << w << "x" << h << "  ch=" << ch << endl;
    }
    else
    {
        cerr << "Erro ao carregar textura: " << path << endl;
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

// ===========================================================================
// loadScene
//
// Formato de cena.txt:
//   camera px py pz yaw pitch
//   light  i  px py pz  r g b  on
//   object arquivo  px py pz  rx ry rz  sx sy sz  [textura]
// ===========================================================================
void loadScene(const string& path, GLuint shaderID)
{
    ifstream file(path.c_str());
    if (!file.is_open())
    {
        cerr << "Erro ao abrir cena: " << path << endl;
        return;
    }

    string line;
    while (getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;

        istringstream ss(line);
        string token;
        ss >> token;

        if (token == "camera")
        {
            float px, py, pz, yaw, pitch;
            ss >> px >> py >> pz >> yaw >> pitch;
            camera = Camera(vec3(px, py, pz), yaw, pitch);
        }
        else if (token == "light")
        {
            int idx;
            float px, py, pz, r, g, b;
            int on;
            ss >> idx >> px >> py >> pz >> r >> g >> b >> on;
            if (idx >= 0 && idx < 3)
            {
                lightPositions[idx] = vec3(px, py, pz);
                lightColors[idx]    = vec3(r, g, b);
                lightEnabled[idx]   = (on != 0);
            }
        }
        else if (token == "object")
        {
            string objPath;
            float px, py, pz, rx, ry, rz, sx, sy, sz;
            ss >> objPath >> px >> py >> pz >> rx >> ry >> rz >> sx >> sy >> sz;

            string texPath;
            ss >> texPath;

            int    nV = 0;
            string mtlFileName;
            int    vao = loadSimpleOBJ(objPath, nV, mtlFileName);
            if (vao == -1) continue;

            Material mat;
            if (!mtlFileName.empty())
            {
                string dir = objPath.substr(0, objPath.find_last_of("/\\") + 1);
                mat = loadMTL(dir + mtlFileName);
            }

            OBJModel obj;
            obj.VAO       = (GLuint)vao;
            obj.nVertices = nV;
            obj.mat       = mat;
            obj.position  = vec3(px, py, pz);
            obj.rotation  = vec3(rx, ry, rz);
            obj.scale     = vec3(sx, sy, sz);

            string base = objPath.substr(objPath.find_last_of("/\\") + 1);
            obj.name = base.substr(0, base.find_last_of('.'));

            if (!texPath.empty())
                obj.textureID = loadTexture(texPath);

            objects.push_back(obj);
        }
    }

    file.close();
    cout << "Cena carregada: " << objects.size() << " objeto(s)\n\n";
}

// ===========================================================================
// updateLights
// ===========================================================================
void updateLights(GLuint shaderID)
{
    for (int i = 0; i < 3; i++)
    {
        string posName = "lightPos[" + to_string(i) + "]";
        string colName = "lightColor[" + to_string(i) + "]";
        string onName  = "lightOn[" + to_string(i) + "]";

        glUniform3fv(glGetUniformLocation(shaderID, posName.c_str()), 1, value_ptr(lightPositions[i]));
        glUniform3fv(glGetUniformLocation(shaderID, colName.c_str()), 1, value_ptr(lightColors[i]));
        glUniform1i (glGetUniformLocation(shaderID, onName.c_str()),  lightEnabled[i] ? 1 : 0);
    }
}

// ===========================================================================
// drawObject
// ===========================================================================
void drawObject(GLuint shaderID, const OBJModel& obj, bool selected)
{
    vec3 color = selected ? mix(obj.mat.Kd, vec3(1.0f), 0.25f)
                          : obj.mat.Kd;
    glUniform3f(glGetUniformLocation(shaderID, "modelColor"), color.r, color.g, color.b);

    glUniform1f(glGetUniformLocation(shaderID, "ka"), obj.mat.ka);
    glUniform1f(glGetUniformLocation(shaderID, "kd"), obj.mat.kd);
    glUniform1f(glGetUniformLocation(shaderID, "ks"), obj.mat.ks);
    glUniform1f(glGetUniformLocation(shaderID, "q"),  obj.mat.q);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, obj.textureID);
    glUniform1i(glGetUniformLocation(shaderID, "texSampler"), 0);
    glUniform1i(glGetUniformLocation(shaderID, "useTexture"),
                (obj.textureID != 0 && globalUseTexture) ? 1 : 0);

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
    if (objects.empty()) return;

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
