// see: https://github.com/emscripten-core/emscripten/pull/3948#issuecomment-744032264
Module.preRun = function customPreRun() {
  ENV = Object.create(process.env);
}
