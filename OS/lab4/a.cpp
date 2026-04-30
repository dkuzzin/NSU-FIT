#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr int kBoardWidth = 10;
constexpr int kBoardHeight = 20;
constexpr int kPieceCells = 4;
constexpr int kPieceTypes = 7;
constexpr int kScreenWidth = 74;
constexpr int kScreenHeight = 34;

enum class InputKey {
    None,
    Left,
    Right,
    Rotate,
    SoftDrop,
    HardDrop,
    Quit
};

enum class Color {
    Default,
    Dim,
    White,
    Cyan,
    Yellow,
    Magenta,
    Green,
    Red,
    Blue
};

struct Cell {
    char ch = ' ';
    Color color = Color::Default;
};

class Canvas {
public:
    Canvas() : pixels_(kScreenHeight, std::vector<Cell>(kScreenWidth)) {}

    void clear() {
        for (auto &row : pixels_) {
            for (auto &cell : row) {
                cell.ch = ' ';
                cell.color = Color::Default;
            }
        }
    }

    void set(int x, int y, char ch, Color color) {
        if (x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight) {
            return;
        }
        pixels_[y][x] = {ch, color};
    }

    void writeText(int x, int y, const std::string &text, Color color) {
        for (size_t i = 0; i < text.size(); ++i) {
            set(x + static_cast<int>(i), y, text[i], color);
        }
    }

    std::string toAnsiString() const {
        std::string out;
        out.reserve(kScreenWidth * kScreenHeight * 8);
        out += "\033[H";

        for (int y = 0; y < kScreenHeight; ++y) {
            Color active = Color::Default;
            out += colorCode(active);

            for (int x = 0; x < kScreenWidth; ++x) {
                const Cell &cell = pixels_[y][x];
                if (cell.color != active) {
                    active = cell.color;
                    out += colorCode(active);
                }
                out += cell.ch;
            }

            out += "\033[0m";
            if (y + 1 != kScreenHeight) {
                out += '\n';
            }
        }

        return out;
    }

private:
    static const char *colorCode(Color color) {
        switch (color) {
            case Color::Dim: return "\033[90m";
            case Color::White: return "\033[97m";
            case Color::Cyan: return "\033[96m";
            case Color::Yellow: return "\033[93m";
            case Color::Magenta: return "\033[95m";
            case Color::Green: return "\033[92m";
            case Color::Red: return "\033[91m";
            case Color::Blue: return "\033[94m";
            case Color::Default: return "\033[0m";
        }
        return "\033[0m";
    }

    std::vector<std::vector<Cell>> pixels_;
};

struct Star {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f;
    char glyph = '.';
    Color color = Color::Dim;
};

class StarField {
public:
    explicit StarField(int count)
        : rng_(static_cast<unsigned int>(
              std::chrono::steady_clock::now().time_since_epoch().count())) {
        stars_.reserve(count);
        for (int i = 0; i < count; ++i) {
            stars_.push_back(makeStar(true));
        }
    }

    void update(float dt) {
        for (auto &star : stars_) {
            star.y += star.speed * dt;
            if (star.y >= static_cast<float>(kScreenHeight)) {
                star = makeStar(false);
            }
        }
    }

    void draw(Canvas &canvas) const {
        for (const auto &star : stars_) {
            canvas.set(static_cast<int>(std::round(star.x)),
                       static_cast<int>(std::round(star.y)),
                       star.glyph, star.color);
        }
    }

private:
    Star makeStar(bool anywhere) {
        std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(kScreenWidth - 1));
        std::uniform_real_distribution<float> y_dist(anywhere ? 0.0f : -8.0f,
                                                     anywhere ? static_cast<float>(kScreenHeight - 1) : -1.0f);
        std::uniform_real_distribution<float> speed_dist(7.0f, 28.0f);
        std::uniform_int_distribution<int> glyph_dist(0, 2);
        std::uniform_int_distribution<int> color_dist(0, 2);

        static const std::array<char, 3> glyphs = {'.', '+', '*'};
        static const std::array<Color, 3> colors = {Color::Dim, Color::White, Color::Blue};

        Star star;
        star.x = x_dist(rng_);
        star.y = y_dist(rng_);
        star.speed = speed_dist(rng_);
        star.glyph = glyphs[glyph_dist(rng_)];
        star.color = colors[color_dist(rng_)];
        return star;
    }

    std::vector<Star> stars_;
    mutable std::mt19937 rng_;
};

struct Piece {
    int x = 0;
    int y = 0;
    int type = 0;
    int rotation = 0;
};

class TetrominoLibrary {
public:
    static constexpr int shapes[kPieceTypes][4][kPieceCells][2] = {
        {
            {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
            {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
            {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
            {{1, 0}, {1, 1}, {1, 2}, {1, 3}}
        },
        {
            {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
            {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
            {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
            {{1, 0}, {2, 0}, {1, 1}, {2, 1}}
        },
        {
            {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
            {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
            {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
            {{1, 0}, {0, 1}, {1, 1}, {1, 2}}
        },
        {
            {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
            {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
            {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
            {{0, 0}, {0, 1}, {1, 1}, {1, 2}}
        },
        {
            {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
            {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
            {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
            {{1, 0}, {0, 1}, {1, 1}, {0, 2}}
        },
        {
            {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
            {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
            {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
            {{1, 0}, {1, 1}, {0, 2}, {1, 2}}
        },
        {
            {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
            {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
            {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
            {{0, 0}, {1, 0}, {1, 1}, {1, 2}}
        }
    };

    static Color colorForType(int type) {
        static const std::array<Color, kPieceTypes> colors = {
            Color::Cyan, Color::Yellow, Color::Magenta, Color::Green,
            Color::Red, Color::Blue, Color::White
        };
        return colors[type];
    }
};

constexpr int TetrominoLibrary::shapes[kPieceTypes][4][kPieceCells][2];

class PieceFactory {
public:
    PieceFactory()
        : rng_(static_cast<unsigned int>(
              std::chrono::steady_clock::now().time_since_epoch().count())),
          dist_(0, kPieceTypes - 1) {}

    Piece next() {
        Piece piece;
        piece.type = dist_(rng_);
        piece.rotation = 0;
        piece.x = kBoardWidth / 2 - 2;
        piece.y = 0;
        return piece;
    }

private:
    std::mt19937 rng_;
    std::uniform_int_distribution<int> dist_;
};

class Board {
public:
    Board() {
        for (auto &row : cells_) {
            row.fill(0);
        }
    }

    bool canPlace(const Piece &piece) const {
        for (const auto &offset : TetrominoLibrary::shapes[piece.type][piece.rotation]) {
            int px = piece.x + offset[0];
            int py = piece.y + offset[1];
            if (px < 0 || px >= kBoardWidth || py < 0 || py >= kBoardHeight) {
                return false;
            }
            if (cells_[py][px] != 0) {
                return false;
            }
        }
        return true;
    }

    void lock(const Piece &piece) {
        for (const auto &offset : TetrominoLibrary::shapes[piece.type][piece.rotation]) {
            int px = piece.x + offset[0];
            int py = piece.y + offset[1];
            cells_[py][px] = piece.type + 1;
        }
    }

    int clearLines() {
        int cleared = 0;

        for (int y = kBoardHeight - 1; y >= 0; --y) {
            bool full = std::all_of(cells_[y].begin(), cells_[y].end(), [](int cell) {
                return cell != 0;
            });
            if (!full) {
                continue;
            }

            ++cleared;
            for (int row = y; row > 0; --row) {
                cells_[row] = cells_[row - 1];
            }
            cells_[0].fill(0);
            ++y;
        }

        return cleared;
    }

    int at(int x, int y) const {
        return cells_[y][x];
    }

private:
    std::array<std::array<int, kBoardWidth>, kBoardHeight> cells_{};
};

class Terminal {
public:
    Terminal() = default;

    bool enter() {
        if (tcgetattr(STDIN_FILENO, &original_) == -1) {
            return false;
        }

        termios raw = original_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
            return false;
        }

        active_ = true;
        std::printf("\033[2J\033[?25l");
        std::fflush(stdout);
        return true;
    }

    ~Terminal() {
        if (active_) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
            std::printf("\033[0m\033[?25h\033[H\033[J");
            std::fflush(stdout);
        }
    }

    InputKey pollInput() const {
        fd_set set;
        timeval timeout{};
        unsigned char c = 0;

        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);

        if (select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout) <= 0) {
            return InputKey::None;
        }

        if (read(STDIN_FILENO, &c, 1) != 1) {
            return InputKey::None;
        }

        switch (c) {
            case 'a':
            case 'A':
                return InputKey::Left;
            case 'd':
            case 'D':
                return InputKey::Right;
            case 'w':
            case 'W':
                return InputKey::Rotate;
            case 's':
            case 'S':
                return InputKey::SoftDrop;
            case ' ':
                return InputKey::HardDrop;
            case 'q':
            case 'Q':
                return InputKey::Quit;
            case '\033':
                return readEscapeSequence();
            default:
                return InputKey::None;
        }
    }

private:
    InputKey readEscapeSequence() const {
        unsigned char seq[2] = {};
        if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1) {
            return InputKey::None;
        }
        if (seq[0] != '[') {
            return InputKey::None;
        }
        switch (seq[1]) {
            case 'A': return InputKey::Rotate;
            case 'B': return InputKey::SoftDrop;
            case 'C': return InputKey::Right;
            case 'D': return InputKey::Left;
            default: return InputKey::None;
        }
    }

    termios original_{};
    bool active_ = false;
};

class Renderer {
public:
    void draw(Canvas &canvas, const StarField &stars, const Board &board, const Piece &current,
              const Piece &next, int score, int lines, int level, bool game_over) const {
        canvas.clear();
        stars.draw(canvas);

        drawFrame(canvas);
        drawBoard(canvas, board, current);
        drawHud(canvas, next, score, lines, level, game_over);
    }

private:
    void drawFrame(Canvas &canvas) const {
        canvas.writeText(2, 1, "COSMIC TETRIS", Color::White);
        canvas.writeText(2, 2, "Black void. Falling stars. Keep the grid alive.", Color::Dim);

        for (int x = 1; x <= 22; ++x) {
            canvas.set(x, 4, x == 1 || x == 22 ? '+' : '-', Color::White);
            canvas.set(x, 25, x == 1 || x == 22 ? '+' : '-', Color::White);
        }

        for (int y = 5; y < 25; ++y) {
            canvas.set(1, y, '|', Color::White);
            canvas.set(22, y, '|', Color::White);
        }
    }

    void drawBlock(Canvas &canvas, int cell_x, int cell_y, Color color) const {
        int x = 2 + cell_x * 2;
        int y = 5 + cell_y;
        canvas.set(x, y, '#', color);
        canvas.set(x + 1, y, '#', color);
    }

    void drawBoard(Canvas &canvas, const Board &board, const Piece &current) const {
        for (int y = 0; y < kBoardHeight; ++y) {
            for (int x = 0; x < kBoardWidth; ++x) {
                int cell = board.at(x, y);
                if (cell != 0) {
                    drawBlock(canvas, x, y, TetrominoLibrary::colorForType(cell - 1));
                } else {
                    int sx = 2 + x * 2;
                    int sy = 5 + y;
                    canvas.set(sx, sy, '.', Color::Dim);
                    canvas.set(sx + 1, sy, '.', Color::Dim);
                }
            }
        }

        for (const auto &offset : TetrominoLibrary::shapes[current.type][current.rotation]) {
            drawBlock(canvas, current.x + offset[0], current.y + offset[1],
                      TetrominoLibrary::colorForType(current.type));
        }
    }

    void drawHud(Canvas &canvas, const Piece &next, int score, int lines, int level,
                 bool game_over) const {
        canvas.writeText(28, 5, "Telemetry", Color::White);
        canvas.writeText(28, 7, "Score : " + std::to_string(score), Color::Cyan);
        canvas.writeText(28, 8, "Lines : " + std::to_string(lines), Color::Green);
        canvas.writeText(28, 9, "Level : " + std::to_string(level), Color::Yellow);

        canvas.writeText(28, 12, "Next Sector", Color::White);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 8; ++j) {
                canvas.set(28 + j, 14 + i, ' ', Color::Default);
            }
        }
        for (const auto &offset : TetrominoLibrary::shapes[next.type][0]) {
            int x = 28 + offset[0] * 2;
            int y = 14 + offset[1];
            canvas.set(x, y, '#', TetrominoLibrary::colorForType(next.type));
            canvas.set(x + 1, y, '#', TetrominoLibrary::colorForType(next.type));
        }

        canvas.writeText(28, 20, "Controls", Color::White);
        canvas.writeText(28, 21, "Left/Right : move", Color::Dim);
        canvas.writeText(28, 22, "Up / W     : rotate", Color::Dim);
        canvas.writeText(28, 23, "Down / S   : soft drop", Color::Dim);
        canvas.writeText(28, 24, "Space      : hard drop", Color::Dim);
        canvas.writeText(28, 25, "Q          : quit", Color::Dim);

        if (game_over) {
            canvas.writeText(28, 28, "Signal lost.", Color::Red);
            canvas.writeText(28, 29, "Press Q to exit.", Color::White);
        } else {
            canvas.writeText(28, 28, "Status: in orbit", Color::Blue);
        }
    }
};

class Game {
public:
    Game()
        : stars_(90) {
        current_ = factory_.next();
        next_ = factory_.next();
    }

    int run() {
        if (!terminal_.enter()) {
            std::perror("termios");
            return EXIT_FAILURE;
        }

        auto last_tick = std::chrono::steady_clock::now();
        auto last_drop = last_tick;

        while (true) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last_tick).count();
            last_tick = now;

            InputKey key = terminal_.pollInput();
            if (key == InputKey::Quit) {
                break;
            }

            if (!game_over_) {
                handleInput(key);
                stars_.update(dt);

                int delay = std::max(90, 550 - (level_ - 1) * 45);
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_drop).count() >= delay) {
                    stepGravity();
                    last_drop = now;
                }
            }

            renderer_.draw(canvas_, stars_, board_, current_, next_, score_, lines_cleared_,
                           level_, game_over_);
            std::string frame = canvas_.toAnsiString();
            std::fwrite(frame.data(), 1, frame.size(), stdout);
            std::fflush(stdout);

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        return EXIT_SUCCESS;
    }

private:
    void handleInput(InputKey key) {
        switch (key) {
            case InputKey::Left:
                tryMove(current_.x - 1, current_.y, current_.rotation);
                break;
            case InputKey::Right:
                tryMove(current_.x + 1, current_.y, current_.rotation);
                break;
            case InputKey::Rotate:
                tryRotate();
                break;
            case InputKey::SoftDrop:
                if (tryMove(current_.x, current_.y + 1, current_.rotation)) {
                    ++score_;
                }
                break;
            case InputKey::HardDrop:
                hardDrop();
                break;
            case InputKey::None:
            case InputKey::Quit:
                break;
        }
    }

    bool tryMove(int x, int y, int rotation) {
        Piece moved = current_;
        moved.x = x;
        moved.y = y;
        moved.rotation = rotation;
        if (!board_.canPlace(moved)) {
            return false;
        }
        current_ = moved;
        return true;
    }

    void tryRotate() {
        static const std::array<int, 5> kicks = {0, -1, 1, -2, 2};
        int next_rotation = (current_.rotation + 1) % 4;
        for (int dx : kicks) {
            if (tryMove(current_.x + dx, current_.y, next_rotation)) {
                return;
            }
        }
    }

    void hardDrop() {
        while (tryMove(current_.x, current_.y + 1, current_.rotation)) {
            score_ += 2;
        }
        settleCurrent();
    }

    void stepGravity() {
        if (!tryMove(current_.x, current_.y + 1, current_.rotation)) {
            settleCurrent();
        }
    }

    void settleCurrent() {
        board_.lock(current_);
        int cleared = board_.clearLines();
        lines_cleared_ += cleared;
        score_ += scoreForLines(cleared);
        level_ = lines_cleared_ / 10 + 1;
        current_ = next_;
        current_.x = kBoardWidth / 2 - 2;
        current_.y = 0;
        current_.rotation = 0;
        next_ = factory_.next();
        if (!board_.canPlace(current_)) {
            game_over_ = true;
        }
    }

    static int scoreForLines(int lines) {
        static const std::array<int, 5> scores = {0, 100, 300, 700, 1500};
        return scores[std::clamp(lines, 0, 4)];
    }

    Terminal terminal_;
    Canvas canvas_;
    StarField stars_;
    Board board_;
    PieceFactory factory_;
    Renderer renderer_;
    Piece current_;
    Piece next_;
    int score_ = 0;
    int lines_cleared_ = 0;
    int level_ = 1;
    bool game_over_ = false;
};

}  // namespace

int main() {
    Game game;
    return game.run();
}
