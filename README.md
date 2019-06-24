# constexpr-everything

A libclang based project to automatically rewrite as much code as possible to be evaluated in `constexpr` contexts.

## Building

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

## Usage

Build a project with a [compilation database](https://clang.llvm.org/docs/JSONCompilationDatabase.html), run `constexpr-everything` on the source files.

## License

constexpr-everything is licensed and distributed under the Apache 2.0 license. [Contact us](mailto:opensource@trailofbits.com) if you're looking for an exception to the terms.
