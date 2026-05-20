/* ObjTex - Carregamento de arquivo .OBJ com coordenadas de textura
 *
 * Baseado nos exemplos de Rossana Baptista Queiroz
 * para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 *
 * Funcionalidades:
 *   - Leitura de arquivo .OBJ: vértices (v), coordenadas de textura (vt) e normais (vn)
 *   - Leitura do arquivo .MTL para obter o nome da textura difusa (map_Kd)
 *   - Atributos de vértice: pos(3) + texcoord(2) + normal(3) = 8 floats/vértice
 *   - Renderização com iluminação de Phong + amostragem de textura
 *   - Teclas X, Y, Z: rotação no respectivo eixo | ESC: fecha
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

// stb_image
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// ---------------------------------------------------------------------------
// Constantes
// ---------------------------------------------------------------------------
const GLuint WIDTH = 800, HEIGHT = 800;

// ---------------------------------------------------------------------------
// Estado de rotação
// ---------------------------------------------------------------------------
bool rotateX = false, rotateY = false, rotateZ = false;

// ---------------------------------------------------------------------------
// Protótipos
// ---------------------------------------------------------------------------
void     key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
int      setupShader();
int      loadSimpleOBJ(string filePATH, int& nVertices, string& mtlFile);
string   loadMTL(string mtlPath);
GLuint   loadTexture(string filePath, int& width, int& height);
void     drawGeometry(GLuint shaderID, GLuint VAO, vec3 position, vec3 dimensions,
                      float angle, int nVertices,
                      vec3 axis = vec3(0.0f, 1.0f, 0.0f));

// ---------------------------------------------------------------------------
// Vertex Shader
// Atributos de entrada:
//   location 0 — position  (xyz)
//   location 1 — texCoord  (st)
//   location 2 — normal    (xyz)
// ---------------------------------------------------------------------------
const GLchar* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 normal;

uniform mat4 projection;
uniform mat4 model;

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 fragPos;

void main()
{
    gl_Position = projection * model * vec4(position, 1.0);
    fragPos     = vec3(model * vec4(position, 1.0));
    vTexCoord   = texCoord;
    // Transforma a normal pelo inverso transposto da matriz de modelo
    // para lidar corretamente com escalonamentos não-uniformes
    vNormal = mat3(transpose(inverse(model))) * normal;
}
)";

// ---------------------------------------------------------------------------
// Fragment Shader — Phong + amostragem de textura difusa
// ---------------------------------------------------------------------------
const GLchar* fragmentShaderSource = R"(
#version 400
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 fragPos;

uniform sampler2D texBuff;
uniform vec3  lightPos;
uniform vec3  camPos;
uniform float ka;
uniform float kd;
uniform float ks;
uniform float q;

out vec4 color;

void main()
{
    vec3 lightColor  = vec3(1.0, 1.0, 1.0);
    vec4 objectColor = texture(texBuff, vTexCoord);

    // Componente ambiente
    vec3 ambient = ka * lightColor;

    // Componente difusa
    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(lightPos - fragPos);
    float diff = max(dot(N, L), 0.0);
    vec3  diffuse = kd * diff * lightColor;

    // Componente especular
    vec3  R    = normalize(reflect(-L, N));
    vec3  V    = normalize(camPos - fragPos);
    float spec = pow(max(dot(R, V), 0.0), q);
    vec3  specular = ks * spec * lightColor;

    vec3 result = (ambient + diffuse) * vec3(objectColor) + specular;
    color = vec4(result, objectColor.a);
}
)";

// ===========================================================================
// MAIN
// ===========================================================================
int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OBJ Texturado - Suzanne", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported: " << version << endl;

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // Compilar shaders
    GLuint shaderID = setupShader();

    // Carregar .OBJ — captura também o nome do arquivo .MTL referenciado
    int    nVertices;
    string mtlFileName;
    int    VAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, mtlFileName);
    if (VAO == -1)
    {
        cerr << "Falha ao carregar o OBJ." << endl;
        glfwTerminate();
        return -1;
    }

    // Carregar .MTL para obter o nome da textura difusa
    string textureName;
    if (!mtlFileName.empty())
        textureName = loadMTL("../assets/Modelos3D/" + mtlFileName);

    if (textureName.empty())
        cerr << "Nenhuma textura (map_Kd) encontrada no MTL." << endl;
    else
        cout << "Textura carregada do MTL: " << textureName << endl;

    // Carregar textura
    int    imgWidth, imgHeight;
    GLuint texID = 0;
    if (!textureName.empty())
        texID = loadTexture("../assets/Modelos3D/" + textureName, imgWidth, imgHeight);

    // Parâmetros de iluminação Phong
    float ka = 0.2f, kd = 0.7f, ks = 0.5f, q = 32.0f;
    vec3  lightPos = vec3(2.0f, 4.0f,  2.0f);
    vec3  camPos   = vec3(0.0f, 0.0f,  0.0f); // câmera na origem, objeto em z=-3

    glUseProgram(shaderID);
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);
    glUniform1f(glGetUniformLocation(shaderID, "ka"),       ka);
    glUniform1f(glGetUniformLocation(shaderID, "kd"),       kd);
    glUniform1f(glGetUniformLocation(shaderID, "ks"),       ks);
    glUniform1f(glGetUniformLocation(shaderID, "q"),        q);
    glUniform3f(glGetUniformLocation(shaderID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform3f(glGetUniformLocation(shaderID, "camPos"),   camPos.x,   camPos.y,   camPos.z);

    // Projeção perspectiva
    mat4 projection = perspective(radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    // Game loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float angle = (float)glfwGetTime();

        vec3 axis = vec3(0.0f, 1.0f, 0.0f); // padrão: Y
        if      (rotateX) axis = vec3(1.0f, 0.0f, 0.0f);
        else if (rotateY) axis = vec3(0.0f, 1.0f, 0.0f);
        else if (rotateZ) axis = vec3(0.0f, 0.0f, 1.0f);

        glBindVertexArray((GLuint)VAO);
        glBindTexture(GL_TEXTURE_2D, texID);

        // Objeto posicionado em z=-3 (câmera na origem, perspectiva)
        drawGeometry(shaderID, (GLuint)VAO, vec3(0.0f, 0.0f, -3.0f), vec3(1.0f), angle, nVertices, axis);

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    GLuint vaoU = (GLuint)VAO;
    glDeleteVertexArrays(1, &vaoU);
    glfwTerminate();
    return 0;
}

// ===========================================================================
// key_callback
// ===========================================================================
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    { rotateX = true;  rotateY = false; rotateZ = false; }

    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    { rotateX = false; rotateY = true;  rotateZ = false; }

    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    { rotateX = false; rotateY = false; rotateZ = true;  }
}

// ===========================================================================
// setupShader
// ===========================================================================
int setupShader()
{
    GLint  success;
    GLchar infoLog[512];

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

// ===========================================================================
// loadSimpleOBJ
//
// Layout do vBuffer: pos(3) + texcoord(2) + normal(3) = 8 floats por vértice
//   location 0 — posição   (xyz)  @ offset 0
//   location 1 — texcoord  (st)   @ offset 3*sizeof(GLfloat)
//   location 2 — normal    (xyz)  @ offset 5*sizeof(GLfloat)
//
// Parâmetros de saída (por referência):
//   nVertices — número de vértices no buffer
//   mtlFile   — nome do arquivo .MTL referenciado pelo .OBJ (linha "mtllib")
//
// Retorna o identificador do VAO, ou -1 em caso de erro.
// ===========================================================================
int loadSimpleOBJ(string filePATH, int& nVertices, string& mtlFile)
{
    vector<vec3>     positions;
    vector<vec2>     texCoords;
    vector<vec3>     normals;
    vector<GLfloat>  vBuffer;

    mtlFile = "";

    ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePATH << endl;
        return -1;
    }

    string line;
    while (getline(arqEntrada, line))
    {
        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "mtllib")
        {
            // Nome do arquivo .MTL associado
            ssline >> mtlFile;
        }
        else if (word == "v")
        {
            vec3 v;
            ssline >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (word == "vt")
        {
            // Coordenadas de textura (s, t)
            vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            vec3 vn;
            ssline >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (word == "f")
        {
            // Cada token pode ser: vi/ti/ni  ou  vi//ni  ou  vi/ti  ou  vi
            while (ssline >> word)
            {
                int vi = 0, ti = 0, ni = 0;
                istringstream ss(word);
                string index;

                if (getline(ss, index, '/')) vi = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index, '/')) ti = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index))      ni = !index.empty() ? stoi(index) - 1 : 0;

                // --- Posição (atributo 0) ---
                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);

                // --- Coordenada de textura (atributo 1) ---
                if (!texCoords.empty())
                {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                }

                // --- Normal (atributo 2) ---
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
    arqEntrada.close();

    cout << "Gerando o buffer de geometria..." << endl;
    cout << "  Vértices lidos: " << positions.size()
         << "  Texcoords: " << texCoords.size()
         << "  Normais: "   << normals.size()   << endl;

    GLuint VBO, VAO;

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Atributo 0 — posição xyz  (3 floats, offset 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1 — texcoord st  (2 floats, offset 3)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Atributo 2 — normal xyz   (3 floats, offset 5)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // pos(3) + texcoord(2) + normal(3) = 8 floats por vértice
    nVertices = (int)(vBuffer.size() / 8);

    return (int)VAO;
}

// ===========================================================================
// loadMTL
//
// Abre o arquivo .MTL e busca pela linha "map_Kd <nome_da_textura>".
// Retorna o nome do arquivo de textura, ou string vazia se não encontrado.
// ===========================================================================
string loadMTL(string mtlPath)
{
    ifstream file(mtlPath.c_str());
    if (!file.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo MTL: " << mtlPath << endl;
        return "";
    }

    string line;
    while (getline(file, line))
    {
        istringstream ss(line);
        string key;
        ss >> key;

        if (key == "map_Kd")
        {
            string texName;
            ss >> texName;
            file.close();
            return texName;
        }
    }

    file.close();
    return "";
}

// ===========================================================================
// loadTexture
//
// Carrega uma imagem com stb_image e cria uma textura OpenGL 2D.
// Suporta RGB (3 canais) e RGBA (4 canais).
// Retorna o identificador da textura (GLuint).
// ===========================================================================
GLuint loadTexture(string filePath, int& width, int& height)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    // Parâmetros de wrapping e filtragem
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int nrChannels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        cout << "Textura carregada: " << filePath
             << " (" << width << "x" << height << ", " << nrChannels << " canais)" << endl;
    }
    else
    {
        cout << "Falha ao carregar textura: " << filePath << endl;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

// ===========================================================================
// drawGeometry
// ===========================================================================
void drawGeometry(GLuint shaderID, GLuint VAO, vec3 position, vec3 dimensions,
                  float angle, int nVertices, vec3 axis)
{
    mat4 model = mat4(1.0f);
    model = translate(model, position);
    model = rotate(model, angle, axis);
    model = scale(model, dimensions);

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));
    glDrawArrays(GL_TRIANGLES, 0, nVertices);
}
