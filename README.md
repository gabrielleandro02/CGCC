# Computação Gráfica - Híbrido

Repositório de exemplos de códigos em C++ utilizando OpenGL moderna (3.3+) criado para a Atividade Acadêmica Computação Gráfica do curso de graduação em Ciência da Computação - modalidade híbrida - da Unisinos. Ele é estruturado para facilitar a organização dos arquivos e a compilação dos projetos utilizando CMake.

## 📂 Estrutura do Repositório

```plaintext
📂 CGCCHibrido/
├── 📂 include/               # Cabeçalhos e bibliotecas de terceiros
│   ├── 📂 glad/              # Cabeçalhos da GLAD (OpenGL Loader)
│   │   ├── glad.h
│   │   ├── 📂 KHR/           # Diretório com cabeçalhos da Khronos (GLAD)
│   │       ├── khrplatform.h
├── 📂 common/                # Código reutilizável entre os projetos
│   ├── glad.c                # Implementação da GLAD
├── 📂 src/                   # Código-fonte dos exemplos e exercícios
│   ├── Hello3D.cpp           # Exemplo básico de renderização com OpenGL
│   ├── ...                   # Outros exemplos e exercícios futuros
├── 📂 build/                 # Diretório gerado pelo CMake (não incluído no repositório)
├── 📂 assets/                # diretório com modelos 3D, texturas, fontes etc
├── 📄 CMakeLists.txt         # Configuração do CMake para compilar os projetos
├── 📄 README.md              # Este arquivo, com a documentação do repositório
├── 📄 GettingStarted.md      # Tutorial detalhado sobre como compilar usando o CMake
```

Siga as instruções detalhadas em [GettingStarted.md](GettingStarted.md) para configurar e compilar o projeto.

## ⚠️ **IMPORTANTE: Baixar a GLAD Manualmente**
Para que o projeto funcione corretamente, é necessário **baixar a GLAD manualmente** utilizando o **GLAD Generator**.

### 🔗 **Acesse o web service do GLAD**:
👉 [GLAD Generator](https://glad.dav1d.de/)

### ⚙️ **Configuração necessária:**
- **API:** OpenGL  
- **Version:** 3.3+ (ou superior compatível com sua máquina)  
- **Profile:** Core  
- **Language:** C/C++  

### 📥 **Baixe e extraia os arquivos:**
Após a geração, extraia os arquivos baixados e coloque-os nos diretórios correspondentes:
- Copie **`glad.h`** para `include/glad/`
- Copie **`khrplatform.h`** para `include/glad/KHR/`
- Copie **`glad.c`** para `common/`

🚨 **Sem esses arquivos, a compilação falhará!** É necessário colocar esses arquivos nos diretórios corretos, conforme a orientação acima.

---

## SceneViewer — Grau B

Visualizador de cenas 3D com iluminação de Phong, texturas, câmera livre, transformações interativas e trajetórias via curva de Bézier cúbica.

### Como compilar

```powershell
cmake -S . -B build
cmake --build build --config Release
```

O executável é gerado em `C:\Users\<usuario>\CGCCBin\SceneViewer.exe`.

### Como rodar

Execute a partir da pasta `build/` para que os caminhos `../assets/` resolvam corretamente.

```powershell
cd build

# Cena padrão (M6 — duas Suzannes)
& "$env:USERPROFILE\CGCCBin\SceneViewer.exe"

# Cena Grau B (carro, estrada, casa, caixa d'água)
& "$env:USERPROFILE\CGCCBin\SceneViewer.exe" "../assets/cena_carro.txt"
```

### Controles

| Tecla | Ação |
|-------|------|
| W A S D | Mover câmera |
| Mouse | Girar câmera |
| TAB | Selecionar próximo objeto |
| T | Modo Translação (padrão) |
| R | Modo Rotação |
| S | Modo Escala |
| Setas ←→↑↓ | Mover objeto (X/Y) ou girar/escalar |
| E / Q | Mover objeto no eixo Z |
| X / Y / Z | Selecionar eixo de rotação |
| 1 / 2 / 3 | Ligar/desligar luzes |
| U | Ligar/desligar texturas |
| P | Gravar waypoint na posição atual |
| F | Iniciar/parar trajetória Bézier |
| G | Limpar waypoints |
| ESC | Fechar |

### Estrutura de assets

```
assets/
  cena.txt               # cena M6 (padrão)
  cena_carro.txt         # cena Grau B
  Modelos3D/             # modelos OBJ, MTL e texturas
  tex/                   # texturas de estrada
```

### Funcionalidades implementadas

- Renderização com shader Phong (ambiente + difuso + especular)
- 3 luzes ponto independentes com atenuação quadrática por distância
- Câmera livre FPS (WASD + mouse)
- Carregamento de cena via arquivo de texto
- Seleção e transformação interativa de objetos (translação, rotação, escala)
- Texturas PNG/JPG com toggle global (tecla U)
- Trajetória por curva de Bézier cúbica segmentada (piecewise)
- Suporte a múltiplas cenas via argumento de linha de comando

