#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <ntfstl/expected.hpp>

using namespace ntf::numdefs;

enum class NetworkError { Timeout, Disconnected, Unknown };

// Struct Move-Only para probar gestión de recursos
struct Payload {
  int id;
  std::unique_ptr<int> data;

  Payload(int i) : id(i), data(std::make_unique<int>(i * 10)) {}

  // Move only
  Payload(Payload&&) = default;
  Payload& operator=(Payload&&) = default;
  Payload(const Payload&) = delete;
  Payload& operator=(const Payload&) = delete;

  bool operator==(const Payload& other) const { return id == other.id && *data == *other.data; }
};

// -----------------------------------------------------------------------------
// TESTS: Construcción y Acceso Básico
// -----------------------------------------------------------------------------
TEST_CASE("Expected: Construcción y Estado Básico", "[expected][basic]") {

  SECTION("Construcción con Valor (Éxito)") {
    ntf::expected<int, NetworkError> e(42);

    REQUIRE(e.has_value());
    REQUIRE(static_cast<bool>(e));
    REQUIRE(*e == 42);
    REQUIRE(e.value() == 42);
    REQUIRE(e.value_or(0) == 42);
  }

  SECTION("Construcción con Error (Unexpected)") {
    ntf::expected<int, NetworkError> e = ntf::unexpected<NetworkError>(NetworkError::Timeout);

    REQUIRE_FALSE(e.has_value());
    REQUIRE_FALSE(static_cast<bool>(e));
    REQUIRE(e.error() == NetworkError::Timeout);
    REQUIRE(e.value_or(100) == 100); // Debe devolver el fallback
  }

  SECTION("Comparación de Igualdad") {
    ntf::expected<int, std::string> e1(10);
    ntf::expected<int, std::string> e2(10);
    ntf::expected<int, std::string> err1 = ntf::unexpected<std::string>("Fail");

    REQUIRE(e1 == e2);
    REQUIRE(e1 != err1);
  }
}

// -----------------------------------------------------------------------------
// TESTS: Excepciones y Bad Access
// -----------------------------------------------------------------------------
TEST_CASE("Expected: Excepciones", "[expected][exception]") {
  ntf::expected<int, std::string> e = ntf::unexpected<std::string>("Critical Error");

  SECTION("Acceso a value() cuando hay error lanza excepción") {
    // La excepción estándar es std::bad_expected_access
    // Ajusta el tipo de excepción si tu implementación usa otra
    REQUIRE_THROWS_AS(e.value(), ntf::bad_expected_access<std::string>);
  }
}

// -----------------------------------------------------------------------------
// TESTS: Tipos Complejos (Move Semantics)
// -----------------------------------------------------------------------------
TEST_CASE("Expected: Tipos Move-Only", "[expected][move]") {
  SECTION("Mover un expected con valor") {
    ntf::expected<Payload, int> src(Payload(5));

    // Move construction
    ntf::expected<Payload, int> dst(std::move(src));

    REQUIRE(dst.has_value());
    REQUIRE(dst->id == 5);
    REQUIRE(*dst->data == 50);

    // src queda en estado válido pero indefinido (típicamente movido)
    // No deberíamos acceder a src->data después de esto
  }

  SECTION("Emplace construct") {
    ntf::expected<Payload, int> e(std::in_place, 10); // Llama al constructor de Payload(10)
    REQUIRE(e.has_value());
    REQUIRE(e->id == 10);
  }
}

// -----------------------------------------------------------------------------
// TESTS: Operaciones Monadicas (and_then, transform, or_else)
// -----------------------------------------------------------------------------
TEST_CASE("Expected: Operaciones Monadicas", "[expected][monadic]") {

  // Función auxiliar: divide, devuelve error si divisor es 0
  auto safe_divide = [](int a, int b) -> ntf::expected<int, std::string> {
    if (b == 0)
      return ntf::unexpected<std::string>("Division by zero");
    return a / b;
  };

  SECTION("transform (map): Modifica el valor si existe") {
    ntf::expected<int, std::string> e(10);

    // Int -> String
    auto res = e.transform([](int val) { return "Number: " + std::to_string(val); });

    REQUIRE(res.has_value());
    REQUIRE(res.value() == "Number: 10");

    // Si hay error, transform no se ejecuta y propaga el error
    ntf::expected<int, std::string> err = ntf::unexpected<std::string>("Original Error");
    auto res_err = err.transform([](int) { return 999; });

    REQUIRE_FALSE(res_err.has_value());
    REQUIRE(res_err.error() == "Original Error");
  }

  SECTION("and_then (bind): Encadena operaciones que pueden fallar") {
    ntf::expected<int, std::string> start(20);

    auto result = start
                    .and_then([&](int val) { return safe_divide(val, 2); })  // 20 / 2 = 10
                    .and_then([&](int val) { return safe_divide(val, 5); }); // 10 / 5 = 2

    REQUIRE(result.has_value());
    REQUIRE(result.value() == 2);
  }

  SECTION("and_then interrumpe cadena en error") {
    ntf::expected<int, std::string> start(20);

    auto result = start
                    .and_then([&](int val) { return safe_divide(val, 0); })  // Error aquí!
                    .and_then([&](int val) { return safe_divide(val, 5); }); // No se ejecuta

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == "Division by zero");
  }

  SECTION("or_else (catch): Recuperación de errores") {
    ntf::expected<int, std::string> e = ntf::unexpected<std::string>("Fail");

    auto recovered = e.or_else([](const std::string& err) -> ntf::expected<int, std::string> {
      if (err == "Fail")
        return 0; // Recuperamos con valor 0
      return ntf::unexpected<std::string>(err);
    });

    REQUIRE(recovered.has_value());
    REQUIRE(recovered.value() == 0);
  }

  SECTION("transform_error: Modifica el tipo de error") {
    ntf::expected<int, int> e = ntf::unexpected<int>(404);

    // Convertimos código de error int a string
    ntf::expected<int, std::string> res =
      e.transform_error([](int code) { return "Error Code: " + std::to_string(code); });

    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == "Error Code: 404");
  }
}

// -----------------------------------------------------------------------------
// TESTS: Especialización void (expected<void, E>)
// -----------------------------------------------------------------------------
TEST_CASE("Expected: Especialización Void", "[expected][void]") {

  SECTION("Éxito sin valor") {
    ntf::expected<void, std::string> e; // Constructor por defecto es éxito void

    REQUIRE(e.has_value());
    // e.value() devuelve void, así que solo verificamos que no lance
    REQUIRE_NOTHROW(e.value());
  }

  SECTION("Fallo en void expected") {
    ntf::expected<void, int> e = ntf::unexpected<int>(500);
    REQUIRE_FALSE(e.has_value());
    REQUIRE(e.error() == 500);
  }

  SECTION("Monadics en void") {
    ntf::expected<void, std::string> e;

    // transform en void no recibe argumentos (el valor es 'nada')
    auto res = e.transform([]() {
      return 100; // void -> int
    });

    REQUIRE(res.has_value());
    REQUIRE(res.value() == 100);
  }
}
