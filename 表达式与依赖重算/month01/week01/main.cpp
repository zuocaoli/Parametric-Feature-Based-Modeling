import std;
import month02.week01.lexer;

int main(int argc, char **argv) {

    using namespace month02::week01;

    try {

        if (argc > 2) {
            throw std::invalid_argument("usage: demo [expression]");
        }

        const auto result = lex(argc == 2 ? argv[1] : "length / 2 + 5");

        if (!result) {
            const auto &error = result.error();
            std::cerr << stage_name(error.stage) << " at byte " << error.position << ": "
                      << error.message << '\n';
            return 1;
        }

        for (const auto &token : *result) {

            std::cout << token_name(token.kind) << " '" << token.text << "' @" << token.position;

            if (token.kind == TokenKind::Number) {
                std::cout << " = " << token.number;
            }
            
            std::cout << '\n';
        }

    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
