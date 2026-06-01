/*
 * fk_level_generator.cpp - Multi-threaded Level Generator for "Free The Key"
 *
 * Original generator: https://github.com/fogleman/rush
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Constants matching Go Engine Configuration
constexpr int MAX_PIECES = 18;
constexpr int GRID_SIZE = 6;
constexpr int BOARD_SIZE = GRID_SIZE * GRID_SIZE;
constexpr int PRIMARY_ROW = 2;
constexpr int PRIMARY_SIZE = 2;
constexpr int MAX_FK_BLOCKS = 12;

// Model Structs
struct Piece {
  int position; // y * Width + x
  int size;
  int orientation; // 0 = Horizontal, 1 = Vertical

  int stride(int w) const { return (orientation == 0) ? 1 : w; }
  int row(int w) const { return position / w; }
  int col(int w) const { return position % w; }
};

struct Move {
  int piece;
  int steps;

  int abs_steps() const { return (steps < 0) ? -steps : steps; }
};

// Memoization Key Structures matching Go implementation
struct MemoKey {
  std::array<int, MAX_PIECES> data;

  MemoKey() { data.fill(0); }

  bool operator==(const MemoKey &other) const { return data == other.data; }

  bool less(const MemoKey &other, bool primary) const {
    int i = primary ? 0 : 1;
    for (; i < MAX_PIECES; ++i) {
      if (data[i] != other.data[i]) {
        return data[i] < other.data[i];
      }
    }
    return false;
  }
};

struct MemoKeyHash {
  std::size_t operator()(const MemoKey &k) const {
    std::size_t h = 0;
    for (int x : k.data) {
      h ^= std::hash<int>{}(x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
  }
};

// Memo class keeping track of searched states
class Memo {
public:
  std::unordered_map<MemoKey, int, MemoKeyHash> data;
  uint64_t hits = 0;

  void clear() {
    data.clear();
    hits = 0;
  }

  int size() const { return data.size(); }

  uint64_t get_hits() const { return hits; }

  bool add(const MemoKey &key, int depth) {
    hits++;
    auto it = data.find(key);
    if (it != data.end() && it->second >= depth) {
      return false;
    }
    data[key] = depth;
    return true;
  }

  void set(const MemoKey &key, int depth) { data[key] = depth; }
};

// Board structure maintaining puzzle state
struct Board {
  int width;
  int height;
  std::vector<Piece> pieces;
  std::vector<int> walls;
  std::vector<bool> occupied;
  MemoKey memo_key;

  Board(int w, int h) : width(w), height(h), occupied(w * h, false) {
    update_memo_key();
  }

  Board()
      : width(GRID_SIZE), height(GRID_SIZE),
        occupied(GRID_SIZE * GRID_SIZE, false) {
    update_memo_key();
  }

  void update_memo_key() {
    memo_key = MemoKey();
    for (size_t i = 0; i < pieces.size() && i < MAX_PIECES; ++i) {
      memo_key.data[i] = pieces[i].position;
    }
  }

  Board copy() const {
    Board b(width, height);
    b.pieces = pieces;
    b.walls = walls;
    b.occupied = occupied;
    b.memo_key = memo_key;
    return b;
  }

  void sort_pieces() {
    if (pieces.size() < 2)
      return;
    std::sort(
        pieces.begin() + 1, pieces.end(),
        [](const Piece &a, const Piece &b) { return a.position < b.position; });
    update_memo_key();
  }

  bool is_occupied(const Piece &piece) const {
    int idx = piece.position;
    int st = piece.stride(width);
    for (int i = 0; i < piece.size; ++i) {
      if (occupied[idx]) {
        return true;
      }
      idx += st;
    }
    return false;
  }

  void set_occupied(const Piece &piece, bool value) {
    int idx = piece.position;
    int st = piece.stride(width);
    for (int i = 0; i < piece.size; ++i) {
      occupied[idx] = value;
      idx += st;
    }
  }

  void add_piece_raw(const Piece &piece) {
    int i = pieces.size();
    pieces.push_back(piece);
    set_occupied(piece, true);
    if (i < MAX_PIECES) {
      memo_key.data[i] = piece.position;
    }
  }

  bool add_piece(const Piece &piece) {
    if (is_occupied(piece)) {
      return false;
    }
    add_piece_raw(piece);
    return true;
  }

  bool add_wall(int i) {
    if (occupied[i]) {
      return false;
    }
    walls.push_back(i);
    occupied[i] = true;
    return true;
  }

  void remove_piece(int i) {
    set_occupied(pieces[i], false);
    int j = pieces.size() - 1;
    pieces[i] = pieces[j];
    if (i < MAX_PIECES) {
      memo_key.data[i] = pieces[i].position;
    }
    pieces.pop_back();
    if (j < MAX_PIECES) {
      memo_key.data[j] = 0;
    }
  }

  void remove_last_piece() { remove_piece(pieces.size() - 1); }

  void remove_wall(int i) {
    occupied[walls[i]] = false;
    walls[i] = walls.back();
    walls.pop_back();
  }

  int target() const {
    const auto &piece = pieces[0];
    int r = piece.row(width);
    return (r + 1) * width - piece.size;
  }

  std::vector<Move> get_moves() const {
    std::vector<Move> moves;
    for (size_t i = 0; i < pieces.size(); ++i) {
      const auto &piece = pieces[i];
      int stride = 0, reverse_steps = 0, forward_steps = 0;
      if (piece.orientation == 1) { // Vertical
        int y = piece.position / width;
        reverse_steps = -y;
        forward_steps = height - piece.size - y;
        stride = width;
      } else { // Horizontal
        int x = piece.position % width;
        reverse_steps = -x;
        forward_steps = width - piece.size - x;
        stride = 1;
      }
      // Reverse direction (negative steps)
      int idx = piece.position - stride;
      for (int steps = -1; steps >= reverse_steps; --steps) {
        if (occupied[idx])
          break;
        moves.push_back({(int)i, steps});
        idx -= stride;
      }
      // Forward direction (positive steps)
      idx = piece.position + piece.size * stride;
      for (int steps = 1; steps <= forward_steps; ++steps) {
        if (occupied[idx])
          break;
        moves.push_back({(int)i, steps});
        idx += stride;
      }
    }
    return moves;
  }

  void do_move(const Move &move) {
    auto &piece = pieces[move.piece];
    int stride = piece.stride(width);

    int idx = piece.position;
    for (int i = 0; i < piece.size; ++i) {
      occupied[idx] = false;
      idx += stride;
    }

    piece.position += stride * move.steps;
    if (move.piece < MAX_PIECES) {
      memo_key.data[move.piece] = piece.position;
    }

    idx = piece.position;
    for (int i = 0; i < piece.size; ++i) {
      occupied[idx] = true;
      idx += stride;
    }
  }

  void undo_move(const Move &move) { do_move({move.piece, -move.steps}); }

  bool validate() const {
    if (width < 3 || height < 3)
      return false;
    if (pieces.empty())
      return false;
    if (pieces.size() > MAX_PIECES)
      return false;
    if (pieces[0].orientation != 0)
      return false;

    std::vector<bool> test_occupied(width * height, false);
    for (int w_idx : walls) {
      if (w_idx < 0 || w_idx >= width * height)
        return false;
      if (test_occupied[w_idx])
        return false;
      test_occupied[w_idx] = true;
    }

    int primary_row = pieces[0].row(width);
    for (size_t i = 0; i < pieces.size(); ++i) {
      const auto &piece = pieces[i];
      int r = piece.row(width);
      int c = piece.col(width);

      if (piece.size < 2)
        return false;
      if (i > 0 && piece.orientation == 0 && r == primary_row)
        return false;

      if (piece.orientation == 0) {
        if (r < 0 || r >= height || c < 0 || c + piece.size > width)
          return false;
      } else {
        if (c < 0 || c >= width || r < 0 || r + piece.size > height)
          return false;
      }

      int idx = piece.position;
      int stride = piece.stride(width);
      for (int j = 0; j < piece.size; ++j) {
        if (test_occupied[idx])
          return false;
        test_occupied[idx] = true;
        idx += stride;
      }
    }
    return true;
  }
};

// Static board analyzer to identify impossible positions early
class StaticAnalyzer {
public:
  std::vector<bool> horz;
  std::vector<bool> vert;
  std::vector<int> positions;
  std::vector<int> sizes;
  std::vector<int> blocked;
  std::vector<int> lens;
  std::vector<int> idx;
  std::vector<int> counts;
  std::vector<int> result;
  std::vector<std::vector<int>> placements;

  StaticAnalyzer() {
    int max_board_size = 16;
    horz.resize(max_board_size * max_board_size, false);
    vert.resize(max_board_size * max_board_size, false);
    positions.resize(max_board_size);
    sizes.resize(max_board_size);
    blocked.resize(max_board_size);
    lens.resize(max_board_size);
    idx.resize(max_board_size);
    counts.resize(max_board_size);
    result.resize(max_board_size);
    placements.resize(max_board_size, std::vector<int>(max_board_size));
  }

  bool impossible(const Board &board) {
    analyze(board);
    int w = board.width;
    const auto &piece = board.pieces[0];
    int i0 = piece.position + piece.size;
    int i1 = (piece.row(w) + 1) * w;
    for (int i = i0; i < i1; ++i) {
      if (horz[i] || vert[i]) {
        return true;
      }
    }
    return false;
  }

  void analyze(const Board &board) {
    std::fill(horz.begin(), horz.end(), false);
    std::fill(vert.begin(), vert.end(), false);
    for (int w_idx : board.walls) {
      horz[w_idx] = true;
      vert[w_idx] = true;
    }
    while (step(board)) {
    }
  }

  bool step(const Board &board) {
    bool changed = false;
    int w = board.width;
    int h = board.height;

    for (int y = 0; y < h; ++y) {
      positions.clear();
      sizes.clear();
      for (const auto &piece : board.pieces) {
        if (piece.orientation == 0 && piece.row(w) == y) {
          positions.push_back(piece.col(w));
          sizes.push_back(piece.size);
        }
      }
      if (positions.empty())
        continue;

      blocked.clear();
      int i0 = y * w;
      for (int i = 0; i < w; ++i) {
        if (horz[i0 + i]) {
          blocked.push_back(i);
        }
      }

      std::vector<int> res =
          compute_blocked_squares(w, positions, sizes, blocked);
      for (int i : res) {
        int idx_coord = i + i0;
        if (!vert[idx_coord]) {
          vert[idx_coord] = true;
          changed = true;
        }
      }
    }

    for (int x = 0; x < w; ++x) {
      positions.clear();
      sizes.clear();
      for (const auto &piece : board.pieces) {
        if (piece.orientation == 1 && piece.col(w) == x) {
          positions.push_back(piece.row(w));
          sizes.push_back(piece.size);
        }
      }
      if (positions.empty())
        continue;

      blocked.clear();
      int i0 = x;
      for (int i = 0; i < h; ++i) {
        if (vert[i0 + i * w]) {
          blocked.push_back(i);
        }
      }

      std::vector<int> res =
          compute_blocked_squares(h, positions, sizes, blocked);
      for (int i : res) {
        int idx_coord = i * w + x;
        if (!horz[idx_coord]) {
          horz[idx_coord] = true;
          changed = true;
        }
      }
    }
    return changed;
  }

  std::vector<int> compute_blocked_squares(int w, std::vector<int> pos,
                                           std::vector<int> sz,
                                           const std::vector<int> &blk) {
    int n = pos.size();
    if (n == 0)
      return {};

    for (int i = 1; i < n; ++i) {
      for (int j = i; j > 0 && pos[j] < pos[j - 1]; --j) {
        std::swap(pos[j], pos[j - 1]);
        std::swap(sz[j], sz[j - 1]);
      }
    }

    lens.resize(n);
    for (int i = 0; i < n; ++i) {
      int p = pos[i];
      int s = sz[i];
      int x0 = 0;
      int x1 = w - s;
      for (int b : blk) {
        if (b < p) {
          x0 = std::max(x0, b + 1);
        }
        if (b > p) {
          x1 = std::min(x1, b - s);
        }
      }
      int d = x1 - x0 + 1;
      if (d <= 0) {
        return {}; // Impossible to place the pieces on this layout
      }
      for (int j = 0; j < d; ++j) {
        placements[i][j] = x0 + j;
      }
      lens[i] = d;
    }

    int count = 0;
    counts.assign(w, 0);
    idx.assign(n, 0);

    while (true) {
      bool ok = true;
      for (int i = 1; i < n; ++i) {
        int j = i - 1;
        if (placements[i][idx[i]] - placements[j][idx[j]] < sz[j]) {
          ok = false;
          break;
        }
      }
      if (ok) {
        count++;
        for (int i = 0; i < n; ++i) {
          int p = placements[i][idx[i]];
          int s = sz[i];
          for (int j = 0; j < s; ++j) {
            counts[p + j]++;
          }
        }
      }

      int i = n - 1;
      for (; i >= 0 && idx[i] == lens[i] - 1; --i) {
        idx[i] = 0;
      }
      if (i < 0) {
        break;
      }
      idx[i]++;
    }

    result.clear();
    for (int i = 0; i < w; ++i) {
      if (counts[i] == count) {
        result.push_back(i);
      }
    }
    return result;
  }
};

// IDA* Solver matching the Go implementation
struct Solution {
  bool solvable = false;
  std::vector<Move> moves;
  int num_moves = 0;
  int num_steps = 0;
  int depth = 0;
  int memo_size = 0;
  uint64_t memo_hits = 0;
};

class Solver {
public:
  Board *board_ptr = nullptr;
  int target = 0;
  Memo memo;
  StaticAnalyzer *sa = nullptr;
  std::vector<Move> path;
  std::vector<std::vector<Move>> moves_buf;

  Solver(StaticAnalyzer *static_analyzer) : sa(static_analyzer) {}

  bool is_solved() const { return board_ptr->pieces[0].position == target; }

  bool search(int depth_val, int max_depth, int previous_piece) {
    int height = max_depth - depth_val;
    if (height == 0) {
      return is_solved();
    }

    if (!memo.add(board_ptr->memo_key, height)) {
      return false;
    }

    const auto &primary = board_ptr->pieces[0];
    int i0 = primary.position + primary.size;
    int i1 = target + primary.size - 1;
    int min_moves = 0;
    for (int i = i0; i <= i1; ++i) {
      if (board_ptr->occupied[i]) {
        min_moves++;
      }
    }
    if (min_moves >= height) {
      return false;
    }

    auto &buf = moves_buf[depth_val];
    buf = board_ptr->get_moves();
    for (const auto &move : buf) {
      if (move.piece == previous_piece) {
        continue;
      }
      board_ptr->do_move(move);
      bool solved = search(depth_val + 1, max_depth, move.piece);
      board_ptr->undo_move(move);
      if (solved) {
        memo.set(board_ptr->memo_key, height - 1);
        path[depth_val] = move;
        return true;
      }
    }
    return false;
  }

  Solution solve_internal(Board &b, bool skip_checks) {
    board_ptr = &b;
    target = b.target();
    memo.clear();

    if (!skip_checks) {
      if (!b.validate()) {
        return Solution{};
      }
      if (sa && sa->impossible(b)) {
        return Solution{};
      }
    }

    if (is_solved()) {
      return Solution{true, {}, 0, 0, 0, 0, 0};
    }

    int previous_memo_size = 0;
    int no_change = 0;
    int cutoff = b.width - b.pieces[0].size;

    for (int i = 1;; ++i) {
      path.assign(i, Move{});
      moves_buf.resize(i);
      if (search(0, i, -1)) {
        int steps = 0;
        for (const auto &m : path) {
          steps += m.abs_steps();
        }
        return Solution{true, path,        (int)path.size(), steps,
                        i,    memo.size(), memo.get_hits()};
      }

      int memo_size = memo.size();
      if (memo_size == previous_memo_size) {
        no_change++;
      } else {
        no_change = 0;
      }
      if (!skip_checks && no_change > cutoff) {
        return Solution{false, {}, 0, 0, i, memo_size, memo.get_hits()};
      }
      previous_memo_size = memo_size;
    }
  }

  Solution solve(Board &b) { return solve_internal(b, false); }

  Solution unsafe_solve(Board &b) { return solve_internal(b, true); }
};

// State Space Unsolver using flat heap-allocated BFS exploration to prevent
// stack overflows
class Unsolver {
public:
  Board board;
  Solver solver;
  Memo memo;
  Board best_board;
  Solution best_solution;

  Unsolver(const Board &b, StaticAnalyzer *sa)
      : board(b.copy()), solver(sa), best_board(b.copy()) {}

  std::pair<Board, Solution> unsolve_internal(bool skip_checks) {
    best_board = board.copy();
    best_solution = solver.solve_internal(board, skip_checks);

    if (!best_solution.solvable) {
      return {best_board, best_solution};
    }

    // Flat, heap-allocated queue exploration replaces recursive DFS search
    std::vector<Board> queue;
    queue.push_back(board.copy());

    memo.clear();
    memo.add(board.memo_key, 0);

    size_t head = 0;
    while (head < queue.size()) {
      Board cur = queue[head++];

      Solution solution = solver.solve_internal(cur, skip_checks);

      bool better = false;
      int dNum_moves = solution.num_moves - best_solution.num_moves;
      int dNum_steps = solution.num_steps - best_solution.num_steps;
      if (dNum_moves >= 0) {
        if (dNum_moves > 0) {
          better = true;
        } else if (dNum_moves == 0 && dNum_steps > 0) {
          better = true;
        } else if (dNum_moves == 0 && dNum_steps == 0 &&
                   cur.memo_key.less(best_board.memo_key, true)) {
          better = true;
        }
      }

      if (better) {
        best_solution = solution;
        best_board = cur.copy();
      }

      std::vector<Move> moves = cur.get_moves();
      for (const auto &move : moves) {
        Board next_board = cur.copy();
        next_board.do_move(move);
        if (memo.add(next_board.memo_key, 0)) {
          queue.push_back(next_board);
        }
      }
    }

    return {best_board, best_solution};
  }

  std::pair<Board, Solution> unsolve() { return unsolve_internal(false); }
};

// Simulation mutation framework matching model.go exactly
inline int random_int(int low, int high, std::mt19937 &rng) {
  std::uniform_int_distribution<int> dist(low, high);
  return dist(rng);
}

struct MutationResult {
  std::function<void()> undo = nullptr;
};

MutationResult mutate_make_move(Board &board, std::mt19937 &rng) {
  std::vector<Move> moves = board.get_moves();
  if (moves.empty())
    return {};
  Move move = moves[random_int(0, moves.size() - 1, rng)];
  board.do_move(move);
  return {[&board, move]() { board.undo_move(move); }};
}

std::pair<Piece, bool> random_piece(const Board &board, int max_attempts,
                                    std::mt19937 &rng) {
  int w = board.width;
  int h = board.height;
  for (int i = 0; i < max_attempts; ++i) {
    int size = 2 + random_int(0, 1, rng); // sizes 2 or 3
    int orientation = random_int(0, 1, rng);
    int x = 0, y = 0;
    if (orientation == 1) { // Vertical
      x = random_int(0, w - 1, rng);
      y = random_int(0, h - size, rng);
    } else { // Horizontal
      x = random_int(0, w - size, rng);
      y = random_int(0, h - 1, rng);
    }
    int position = y * w + x;
    Piece p{position, size, orientation};
    if (!board.is_occupied(p)) {
      return {p, true};
    }
  }
  return {Piece{}, false};
}

MutationResult mutate_add_piece(Board &board, int max_attempts,
                                std::mt19937 &rng, int max_pieces_limit) {
  if ((int)board.pieces.size() >= max_pieces_limit)
    return {};
  auto p_res = random_piece(board, max_attempts, rng);
  if (!p_res.second)
    return {};
  int idx = board.pieces.size();
  if (!board.add_piece(p_res.first)) {
    return {};
  }
  return {[&board, idx]() { board.remove_piece(idx); }};
}

MutationResult mutate_remove_piece(Board &board, std::mt19937 &rng) {
  if (board.pieces.size() < 2)
    return {};
  int i = random_int(1, board.pieces.size() - 1, rng);
  Piece p = board.pieces[i];
  board.remove_piece(i);
  return {[&board, p]() { board.add_piece(p); }};
}

MutationResult mutate_remove_and_add_piece(Board &board, int max_attempts,
                                           std::mt19937 &rng,
                                           int max_pieces_limit) {
  auto undo_remove = mutate_remove_piece(board, rng);
  if (!undo_remove.undo)
    return {};
  auto undo_add = mutate_add_piece(board, max_attempts, rng, max_pieces_limit);
  if (!undo_add.undo) {
    return undo_remove;
  }
  return {[undo_add, undo_remove]() {
    undo_add.undo();
    undo_remove.undo();
  }};
}

MutationResult mutate_board(Board &board, int max_attempts, std::mt19937 &rng,
                            int max_pieces_limit) {
  while (true) {
    int choice = random_int(0, 9, rng);
    MutationResult res;
    if (choice == 0) {
      res = mutate_add_piece(board, max_attempts, rng, max_pieces_limit);
    } else if (choice == 1) {
      res = mutate_remove_piece(board, rng);
    } else if (choice == 2) {
      res = mutate_remove_and_add_piece(board, max_attempts, rng,
                                        max_pieces_limit);
    } else {
      res = mutate_make_move(board, rng);
    }
    if (res.undo) {
      return res;
    }
  }
}

double compute_energy(Board &board, Solver &solver) {
  Solution solution = solver.solve(board);
  if (!solution.solvable) {
    return 1.0;
  }
  double e = (double)solution.num_moves;
  e += (double)solution.num_steps / 100.0;
  return -e;
}

// Simulated Annealing matching anneal.go
Board anneal(Board state, double max_temp, double min_temp, int steps,
             std::mt19937 &rng, int max_pieces_limit) {
  double factor = -std::log(max_temp / min_temp);
  Board current = state.copy();
  Board best_state = state.copy();

  StaticAnalyzer sa;
  Solver solver(&sa);

  double best_energy = compute_energy(current, solver);
  double previous_energy = best_energy;
  auto best_time = std::chrono::steady_clock::now();

  std::uniform_real_distribution<double> dist_real(0.0, 1.0);

  for (int step = 0; step < steps; ++step) {
    double pct = (double)step / (double)(steps - 1);
    double temp = max_temp * std::exp(factor * pct);

    auto res = mutate_board(current, 100, rng, max_pieces_limit);
    double energy = compute_energy(current, solver);
    double change = energy - previous_energy;

    if (change > 0 && std::exp(-change / temp) < dist_real(rng)) {
      res.undo();
    } else {
      previous_energy = energy;
      if (energy < best_energy) {
        best_energy = energy;
        best_state = current.copy();
        best_time = std::chrono::steady_clock::now();
      }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - best_time)
                       .count();
    if (elapsed > 15) {
      break;
    }
  }
  return best_state;
}

// Task structuring
struct LevelTask {
  int id;
  int min_pieces;
  int max_pieces;
  int min_moves;
  int max_moves;
};

struct GeneratedLevel {
  Board board;
  int moves;
};

GeneratedLevel generate_single_level(const LevelTask &task, std::mt19937 &rng) {
  StaticAnalyzer sa;
  Solver solver(&sa);

  int current_min_moves = task.min_moves;
  int current_max_moves = task.max_moves;
  int cycle_count = 0;

  while (true) {
    cycle_count++;
    if (cycle_count > 150) {
      if (current_min_moves > 5)
        current_min_moves -= 3;
      current_max_moves += 5;
      cycle_count = 0;
    }

    Board board(GRID_SIZE, GRID_SIZE);
    board.add_piece(Piece{PRIMARY_ROW * GRID_SIZE, PRIMARY_SIZE, 0});

    double max_temp = 22.0;
    double min_temp = 0.5;
    int annealing_steps = (task.min_moves >= 60) ? 600 : 200;

    board = anneal(board, max_temp, min_temp, annealing_steps, rng,
                   task.max_pieces);

    Unsolver unsolver(board, &sa);
    auto unsolve_result = unsolver.unsolve();
    Board final_board = unsolve_result.first;
    Solution final_solution = unsolve_result.second;

    int p_count = (int)final_board.pieces.size();

    if (final_solution.solvable && p_count >= task.min_pieces &&
        p_count <= task.max_pieces &&
        final_solution.num_moves >= current_min_moves &&
        final_solution.num_moves <= current_max_moves) {
      final_board.sort_pieces();
      return {final_board, final_solution.num_moves};
    }
  }
}

int main() {
  std::cout << "=================================================="
            << std::endl;
  std::cout << "                Level Generator                   "
            << std::endl;
  std::cout << "=================================================="
            << std::endl;

  std::vector<LevelTask> tasks;
  for (int i = 1; i <= 100; i++) {
    if (i <= 10)
      tasks.push_back({i, 4, 5, 10, 15});
    else if (i <= 45)
      tasks.push_back({i, 5, 7, 16, 20});
    else
      tasks.push_back({i, 6, 9, 21, 55});
  }

  std::vector<GeneratedLevel> results(100);
  std::atomic<int> shared_task_index(0);
  std::atomic<int> output_counter(0);
  std::mutex tracking_mutex;

  unsigned int total_workers = std::thread::hardware_concurrency();
  if (total_workers == 0)
    total_workers = 4;
  std::cout << "Launching parallel execution threads: " << total_workers
            << std::endl;

  auto calculation_start = std::chrono::high_resolution_clock::now();
  std::vector<std::thread> operational_pool;

  for (unsigned int worker_id = 0; worker_id < total_workers; worker_id++) {
    operational_pool.emplace_back([&, worker_id]() {
      auto timestamp =
          std::chrono::high_resolution_clock::now().time_since_epoch().count();
      std::mt19937 engine(static_cast<unsigned int>(timestamp) +
                          worker_id * 73856093);

      while (true) {
        int target_id = shared_task_index.fetch_add(1);
        if (target_id >= 100)
          break;

        results[target_id] = generate_single_level(tasks[target_id], engine);
        int progress = output_counter.fetch_add(1) + 1;

        std::lock_guard<std::mutex> system_lock(tracking_mutex);
        std::cout << "\rProcessing Map Pipeline: " << progress
                  << "/100 steps completed." << std::flush;
      }
    });
  }

  for (auto &worker : operational_pool) {
    worker.join();
  }

  auto calculation_end = std::chrono::high_resolution_clock::now();
  double execution_seconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(calculation_end -
                                                            calculation_start)
          .count() /
      1000.0;

  std::cout << "\n\nMap Matrix acquired in: " << execution_seconds
            << " seconds." << std::endl;
  std::cout << "Sorting output by optimal move distribution..." << std::endl;

  std::sort(results.begin(), results.end(),
            [](const GeneratedLevel &a, const GeneratedLevel &b) {
              return a.moves < b.moves;
            });

  std::cout << "Writing static map array code into include/FKLevels.h..."
            << std::endl;

  std::system("mkdir -p include");

  std::ofstream file_stream("include/FKLevels.h");
  file_stream << "/*\n"
              << " * FKLevels.h - Static Prebuilt Levels Header Database\n"
              << " * Sorted in ascending order of optimal moves (difficulty).\n"
              << " */\n\n"
              << "#ifndef LEVELS_H\n"
              << "#define LEVELS_H\n\n"
              << "#include <Arduino.h>\n\n"
              << "#ifndef MAX_FK_BLOCKS\n"
              << "#define MAX_FK_BLOCKS 12\n"
              << "#endif\n\n"
              << "struct FKBlock {\n"
              << "  int8_t x, y;\n"
              << "  int8_t w, h;\n"
              << "  bool isKey;\n"
              << "  bool active;\n"
              << "};\n\n"
              << "struct PrebuiltLevel {\n"
              << "  uint8_t blockCount;\n"
              << "  uint8_t minMoves;\n"
              << "  FKBlock blocks[MAX_FK_BLOCKS];\n"
              << "};\n\n"
              << "const PrebuiltLevel PREBUILT_LEVELS[100] PROGMEM = {\n";

  for (size_t map_idx = 0; map_idx < results.size(); map_idx++) {
    const auto &level = results[map_idx];
    file_stream << "  {\n"
                << "    " << level.board.pieces.size() << ", // Level "
                << map_idx + 1 << " total items\n"
                << "    " << level.moves << ", // Minimum moves required\n"
                << "    {\n";

    for (size_t block_idx = 0; block_idx < level.board.pieces.size();
         block_idx++) {
      const auto &block = level.board.pieces[block_idx];
      int width = (block.orientation == 0) ? block.size : 1;
      int height = (block.orientation == 0) ? 1 : block.size;
      std::string identity = (block_idx == 0) ? "true" : "false";
      int grid_x = block.position % GRID_SIZE;
      int grid_y = block.position / GRID_SIZE;

      file_stream << "      {" << grid_x << ", " << grid_y << ", " << width
                  << ", " << height << ", " << identity << ", true}";
      if (block_idx < level.board.pieces.size() - 1 ||
          level.board.pieces.size() < MAX_FK_BLOCKS) {
        file_stream << ",";
      }
      file_stream << " // Entity " << block_idx << "\n";
    }

    for (int filler_idx = level.board.pieces.size(); filler_idx < MAX_FK_BLOCKS;
         filler_idx++) {
      file_stream << "      {0, 0, 0, 0, false, false}";
      if (filler_idx < MAX_FK_BLOCKS - 1)
        file_stream << ",";
      file_stream << "\n";
    }

    file_stream << "    }\n";
    file_stream << (map_idx < 99 ? "  },\n" : "  }\n");
  }
  file_stream << "};\n\n#endif\n";
  file_stream.close();

  std::cout << "Header compilation successful! System ready." << std::endl;
  return 0;
}
