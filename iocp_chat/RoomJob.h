#pragma once
#include <functional>

// Job은 반환값없고 매개변수없는 함수 객체(만들때는 람다로 만들어야함 안그러면 매개변수 넣기 곤란함)
using Job = std::function<void()>;