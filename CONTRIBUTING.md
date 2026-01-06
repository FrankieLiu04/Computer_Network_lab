# Contributing Guide

[![中文](https://img.shields.io/badge/lang-中文-red.svg)](CONTRIBUTING.zh-CN.md)

Thank you for your interest in contributing to this project! Here's how you can help.

## 🌿 Branching Strategy

We use a simplified Git Flow model:

| Branch | Purpose | Protection |
|--------|---------|------------|
| `main` | Stable releases | Protected, PR required |
| `develop` | Development integration | PR required |
| `feature/*` | New features | Personal branches |
| `bugfix/*` | Bug fixes | Personal branches |

### Branch Naming Convention

```
feature/add-compression
feature/file-encryption
bugfix/fix-connection-timeout
bugfix/memory-leak-fix
docs/update-readme
```

## 📝 How to Contribute

### 1. Fork & Clone

```bash
# After forking the repository
git clone https://github.com/YOUR_USERNAME/Computer_Network_lab.git
cd Computer_Network_lab
git remote add upstream https://github.com/FrankieLiu04/Computer_Network_lab.git
```

### 2. Create a Feature Branch

```bash
git checkout develop
git pull upstream develop
git checkout -b feature/your-feature-name
```

### 3. Develop & Commit

```bash
# Make your changes...
git add .
git commit -m "feat: add your feature description"
```

#### Commit Message Convention

We follow [Conventional Commits](https://www.conventionalcommits.org/) specification:

| Type | Description |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation update |
| `style` | Code style (no logic change) |
| `refactor` | Code refactoring |
| `test` | Test related |
| `chore` | Build/tooling changes |

Examples:
```
feat: add resumable file upload
fix: resolve memory overflow for large files
docs: update Docker deployment guide
```

### 4. Push & Create PR

```bash
git push origin feature/your-feature-name
```

Then create a Pull Request on GitHub targeting the `develop` branch.

## ✅ PR Checklist

Before submitting your PR, please verify:

- [ ] Code compiles successfully
- [ ] All tests pass (`ctest` or CI)
- [ ] Code follows style guidelines
- [ ] Documentation updated (if needed)
- [ ] Commit messages follow conventions

## 🏗️ Development Environment

### Requirements

- CMake 3.16+
- C++17 compatible compiler
- Docker (optional, for containerized testing)

### Build

```bash
cd reliable-remote-backup-system
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Test

```bash
cd build
ctest --output-on-failure
```

## 💬 Getting Help

- Questions? Create an [Issue](https://github.com/FrankieLiu04/Computer_Network_lab/issues)
- Want to discuss? Use the `question` label

## 📜 Code of Conduct

Please read our [Code of Conduct](CODE_OF_CONDUCT.md) to maintain a friendly and inclusive environment.
