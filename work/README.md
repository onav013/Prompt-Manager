<div align="center">
  <h1>🧠 Prompt Manager (C)</h1>
  <p>A lightning-fast, terminal-based prompt library and execution engine written in pure C.</p>

  <p>
    <img src="https://img.shields.io/badge/Language-C11-00599C.svg?style=flat-square" alt="Language" />
    <img src="https://img.shields.io/badge/Build-CMake-064F8C.svg?style=flat-square" alt="CMake" />
    <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg?style=flat-square" alt="Platform" />
    <img src="https://img.shields.io/badge/License-MIT-green.svg?style=flat-square" alt="License" />
  </p>
</div>

<br/>

**Prompt Manager** is a lightweight command-line utility designed for developers and AI enthusiasts who need a blazing-fast way to organize, search, and execute LLM prompts directly from their terminal without leaving their workflow.

## ✨ Features

- ⚡ **Zero-Bloat**: Written in pure C with zero heavy dependencies.
- 🗂️ **Tag-Based Organization**: Group prompts logically with multiple tags.
- 🔍 **Instant Search**: Query across prompt names, content, and specific tags.
- 🚀 **One-Click Execution**: Integrated OpenAI-compatible client. Pipe prompts straight to models like DeepSeek, GPT-4, etc.
- 💾 **Local Persistence**: Automatically saves your prompts to a local `.db` file.
- 🎨 **Beautiful CLI**: Clean, colored terminal UI that respects your workspace.

---

## 🛠️ Quick Start

### Prerequisites
- A C11 compatible compiler (GCC, Clang, or MSVC)
- CMake (>= 3.16)

### 1. Build

**Windows (PowerShell / MSVC):**
```powershell
cmake -S . -B build
cmake --build build --config Release
```

**Linux / macOS:**
```bash
cmake -S . -B build
cmake --build build
```

### 2. Run
```bash
# On Windows
.\build\Release\prompt_manager.exe

# On Linux / macOS
./build/prompt_manager
```

---

## 🤖 LLM Configuration

To use the **"Execute via LLM API"** feature, set the following environment variables before running the application. It supports any API compatible with OpenAI's Chat Completions endpoint.

### Example: DeepSeek

**Windows (PowerShell):**
```powershell
$env:PM_API_URL="https://api.deepseek.com"
$env:PM_API_KEY="sk-your-api-key-here"
$env:PM_MODEL="deepseek-chat"
```

**Linux / macOS (Bash/Zsh):**
```bash
export PM_API_URL="https://api.deepseek.com"
export PM_API_KEY="sk-your-api-key-here"
export PM_MODEL="deepseek-chat"
```
> *Note: The manager will automatically append `/v1/chat/completions` if you provide a root URL.*

---

## 🏗️ Project Structure

The codebase is organized following standard C project conventions, making it easy to fork and extend:

```text
prompt-manager/
├── include/           # Header files exposing the public API
├── src/               # Core implementation (Storage, LLM Client, UI, CLI)
├── tests/             # Unit tests using CTest
├── examples/          # Sample prompt data
├── docs/              # Roadmap and documentation
└── CMakeLists.txt     # Build configuration
```

To run the test suite:
```bash
ctest --test-dir build --output-on-failure -C Release
```

---

## 💡 Troubleshooting

- **`Cannot parse model response`**: Double-check your API Key and URL. Ensure your endpoint returns standard JSON (e.g., `"content": "..."`).
- **`cmake: command not found`**: Install CMake and ensure it is added to your system `PATH`.
- **IntelliSense Errors in IDE**: If your IDE complains about missing headers like `cli.h`, ensure you have configured your C/C++ extension to include the `include/` directory, or simply use the CMake Tools extension.

---

## 🤝 Contributing

We welcome contributions! Whether it's fixing bugs, improving the UI, or adding cross-platform HTTP support (e.g., `libcurl`), please check out our guidelines:

- [Contributing Guidelines](CONTRIBUTING.md)
- [Code of Conduct](CODE_OF_CONDUCT.md)
- [Development Roadmap](docs/ROADMAP.md)
- [Security Policy](SECURITY.md)

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
