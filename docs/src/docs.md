# Docs & Doxygen

Markdown documentation is built using `mdbook` and API documentation is built using `doxygen`.

## Markdown documentation (docs)

### Requirements

The markdown documentation is built using [`mdbook`](https://github.com/rust-lang/mdBook).

### Build command

To build the markdown docs use the `docs` target in CMake

```sh
cmake -B build
cmake --build build -- docs
```

## API documentation (doxygen)

This is a link to a doxygen autogenerated code reference.
[Doxygen](./doxygen/html/files.html)

### Requirements

The API documentation is built using `doxygen`, here are the needed tools for building it:

- [`doxygen`](https://www.doxygen.nl/index.html)
- [`dot` from `graphviz`](https://graphviz.org/)

### Build command

To build the API docs use the `doxygen` target in CMake

```sh
cmake -B build
cmake --build build -- doxygen
```

### Serving the docs locally

To start a server for local viewing of the docs, after building, run:

```sh
mdbook serve build/docs
```

`mdbook` will start a local server at `http://localhost:3000` with the built docs.
