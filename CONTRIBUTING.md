# Contributing

## Development Setup

1. Configure:

```bash
cmake -S . -B build
```

2. Build:

```bash
cmake --build build --config Release
```

3. Run tests:

```bash
ctest --test-dir build --output-on-failure -C Release
```

## Pull Request Checklist

- Keep changes focused and small.
- Ensure the project builds successfully.
- Ensure tests pass locally.
- Update README/docs when behavior changes.

## Commit Style

- `feat: ...` for new features
- `fix: ...` for bug fixes
- `docs: ...` for documentation
- `test: ...` for tests
