/*
 * fk_level_generator.cpp - Multi-threaded Level Generator for "Free The Key"
 * Optimized with Bitwise State Packing, Flat BFS Space Explorer, Simulated
 * Annealing, and Thread-Safe Task Distribution.
 *
 * Original generator: https://github.com/fogleman/rush
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#define MAX_FK_BLOCKS 12
#define GRID_SIZE 6
#define BOARD_SIZE (GRID_SIZE * GRID_SIZE)
#define PRIMARY_PIECE_ROW 2
#define PRIMARY_PIECE_SIZE 3 // 1x3 key piece
#define MAX_STATES_LIMIT 800

struct Piece {
  int8_t position;    // y * 6 + x
  int8_t size;        // 2 or 3
  int8_t orientation; // 0 for Horizontal, 1 for Vertical
};

struct Move {
  int pieceIdx;
  int steps;
};

struct Board {
  std::vector<Piece> pieces;
  bool occupied[BOARD_SIZE] = {false};

  void initEmpty() {
    pieces.clear();
    for (int i = 0; i < BOARD_SIZE; i++)
      occupied[i] = false;
  }

  int getStride(const Piece &p) const {
    return (p.orientation == 0) ? 1 : GRID_SIZE;
  }

  bool isOccupied(const Piece &p) const {
    int idx = p.position;
    int stride = getStride(p);
    for (int i = 0; i < p.size; i++) {
      if (idx < 0 || idx >= BOARD_SIZE || occupied[idx])
        return true;
      idx += stride;
    }
    return false;
  }

  void setOccupied(const Piece &p, bool val) {
    int idx = p.position;
    int stride = getStride(p);
    for (int i = 0; i < p.size; i++) {
      if (idx >= 0 && idx < BOARD_SIZE)
        occupied[idx] = val;
      idx += stride;
    }
  }

  bool addPiece(const Piece &p) {
    if (isOccupied(p))
      return false;
    pieces.push_back(p);
    setOccupied(p, true);
    return true;
  }

  void removePiece(int idx) {
    setOccupied(pieces[idx], false);
    pieces.erase(pieces.begin() + idx);
  }
};

// Pack board state into a compact 64-bit unsigned integer (6 bits per position)
uint64_t getBoardKey(const Board &b) {
  uint64_t key = 0;
  int count = b.pieces.size() < 10 ? b.pieces.size() : 10;
  for (int i = 0; i < count; i++) {
    key |= ((uint64_t)(b.pieces[i].position & 0x3F)) << (i * 6);
  }
  return key;
}

Board reconstructBoard(const Board &template_board, uint64_t key) {
  Board b = template_board;
  for (size_t i = 0; i < b.pieces.size(); i++) {
    b.pieces[i].position = (key >> (i * 6)) & 0x3F;
  }
  for (int i = 0; i < BOARD_SIZE; i++)
    b.occupied[i] = false;
  for (const auto &p : b.pieces) {
    b.setOccupied(p, true);
  }
  return b;
}

void sortPiecesCanonical(Board &b) {
  if (b.pieces.size() <= 2)
    return;
  std::sort(
      b.pieces.begin() + 1, b.pieces.end(),
      [](const Piece &x, const Piece &y) { return x.position < y.position; });
}

// Low-level register-friendly state expansion via bit manipulation
std::vector<uint64_t> getNextStates(uint64_t state,
                                    const std::vector<Piece> &pieces_template) {
  bool occupied[BOARD_SIZE] = {false};
  for (size_t idx = 0; idx < pieces_template.size(); idx++) {
    const auto &p = pieces_template[idx];
    int pos = (state >> (idx * 6)) & 0x3F;
    int stride = (p.orientation == 0) ? 1 : GRID_SIZE;
    for (int i = 0; i < p.size; i++) {
      occupied[pos] = true;
      pos += stride;
    }
  }

  std::vector<uint64_t> next_states;
  next_states.reserve(32);

  for (size_t idx = 0; idx < pieces_template.size(); idx++) {
    const auto &p = pieces_template[idx];
    int pos = (state >> (idx * 6)) & 0x3F;
    int px = pos % GRID_SIZE;
    int py = pos / GRID_SIZE;

    if (p.orientation == 0) { // Horizontal
      // Slide Left
      int tx = px - 1;
      while (tx >= 0) {
        int target_pos = py * GRID_SIZE + tx;
        if (occupied[target_pos])
          break;
        uint64_t new_state = state;
        new_state &= ~((uint64_t)0x3F << (idx * 6));
        new_state |= ((uint64_t)target_pos << (idx * 6));
        next_states.push_back(new_state);
        tx--;
      }
      // Slide Right
      tx = px + p.size;
      while (tx < GRID_SIZE) {
        int target_pos = py * GRID_SIZE + tx;
        if (occupied[target_pos])
          break;
        int final_pos = py * GRID_SIZE + (tx - p.size + 1);
        uint64_t new_state = state;
        new_state &= ~((uint64_t)0x3F << (idx * 6));
        new_state |= ((uint64_t)final_pos << (idx * 6));
        next_states.push_back(new_state);
        tx++;
      }
    } else { // Vertical
      // Slide Up
      int ty = py - 1;
      while (ty >= 0) {
        int target_pos = ty * GRID_SIZE + px;
        if (occupied[target_pos])
          break;
        uint64_t new_state = state;
        new_state &= ~((uint64_t)0x3F << (idx * 6));
        new_state |= ((uint64_t)target_pos << (idx * 6));
        next_states.push_back(new_state);
        ty--;
      }
      // Slide Down
      ty = py + p.size;
      while (ty < GRID_SIZE) {
        int target_pos = ty * GRID_SIZE + px;
        if (occupied[target_pos])
          break;
        int final_pos = (ty - p.size + 1) * GRID_SIZE + px;
        uint64_t new_state = state;
        new_state &= ~((uint64_t)0x3F << (idx * 6));
        new_state |= ((uint64_t)final_pos << (idx * 6));
        next_states.push_back(new_state);
        ty++;
      }
    }
  }
  return next_states;
}

struct ExplorerResult {
  bool solvable = false;
  int maxMoves = 0;
  Board hardestBoard;
};

ExplorerResult exploreBoard(const Board &b) {
  ExplorerResult res;
  int primary_row = b.pieces[0].position / GRID_SIZE;
  int target_pos = primary_row * GRID_SIZE + GRID_SIZE - b.pieces[0].size;

  uint64_t start_state = getBoardKey(b);

  // Forward BFS to build state space
  std::unordered_map<uint64_t, int16_t> visited;
  std::queue<uint64_t> q;

  visited[start_state] = -1;
  q.push(start_state);

  while (!q.empty()) {
    uint64_t cur = q.front();
    q.pop();

    if (visited.size() > MAX_STATES_LIMIT) {
      return res; // Safety Limit
    }

    for (uint64_t nxt : getNextStates(cur, b.pieces)) {
      if (visited.find(nxt) == visited.end()) {
        visited[nxt] = -1;
        q.push(nxt);
      }
    }
  }

  // Collect solved states
  std::vector<uint64_t> solved_states;
  for (const auto &pair : visited) {
    int pos = pair.first & 0x3F;
    if (pos == target_pos) {
      solved_states.push_back(pair.first);
    }
  }

  if (solved_states.empty())
    return res;

  // Backward BFS from solved states
  std::queue<uint64_t> bq;
  for (uint64_t s : solved_states) {
    visited[s] = 0;
    bq.push(s);
  }

  while (!bq.empty()) {
    uint64_t cur = bq.front();
    int cur_dist = visited[cur];
    bq.pop();

    for (uint64_t nxt : getNextStates(cur, b.pieces)) {
      auto it = visited.find(nxt);
      if (it != visited.end() && it->second == -1) {
        it->second = cur_dist + 1;
        bq.push(nxt);
      }
    }
  }

  int max_dist = -1;
  uint64_t hardest_state = start_state;
  for (const auto &pair : visited) {
    if (pair.second > max_dist) {
      max_dist = pair.second;
      hardest_state = pair.first;
    }
  }

  if (max_dist >= 0) {
    res.solvable = true;
    res.maxMoves = max_dist;
    res.hardestBoard = reconstructBoard(b, hardest_state);
  }
  return res;
}

bool getRandomPiece(const Board &b, Piece &outPiece, std::mt19937 &rng) {
  std::uniform_int_distribution<int> dist_size(2, 3);
  std::uniform_int_distribution<int> dist_orientation(0, 1);
  std::uniform_int_distribution<int> dist_coord(0, 5);

  for (int attempt = 0; attempt < 80; attempt++) {
    int size = dist_size(rng);
    int orientation = dist_orientation(rng);
    int x, y;
    if (orientation == 0) { // Horizontal
      x = dist_coord(rng) % (GRID_SIZE - size + 1);
      y = dist_coord(rng);
    } else { // Vertical
      x = dist_coord(rng);
      y = dist_coord(rng) % (GRID_SIZE - size + 1);
    }

    int pos = y * GRID_SIZE + x;
    if (orientation == 0 && y == PRIMARY_PIECE_ROW)
      continue; // Row 2 limitation to avoid blocking key piece path

    Piece p{(int8_t)pos, (int8_t)size, (int8_t)orientation};
    if (!b.isOccupied(p)) {
      outPiece = p;
      return true;
    }
  }
  return false;
}

struct MutationUndo {
  enum Type { NONE, ADD, REMOVE, MOVE } type = NONE;
  int pieceIdx;
  Piece oldPiece;
  uint64_t oldStateKey;
};

MutationUndo mutateBoard(Board &b, int max_pieces, std::mt19937 &rng) {
  std::uniform_int_distribution<int> dist_choice(0, 5);
  int choice = dist_choice(rng);
  MutationUndo undo;

  if (choice == 0) { // Add
    if (b.pieces.size() < max_pieces) {
      Piece p;
      if (getRandomPiece(b, p, rng)) {
        b.addPiece(p);
        undo.type = MutationUndo::ADD;
        undo.pieceIdx = b.pieces.size() - 1;
      }
    }
  } else if (choice == 1) { // Remove
    if (b.pieces.size() > 1) {
      std::uniform_int_distribution<int> dist_idx(1, b.pieces.size() - 1);
      int idx = dist_idx(rng);
      undo.type = MutationUndo::REMOVE;
      undo.oldPiece = b.pieces[idx];
      undo.pieceIdx = idx;
      b.removePiece(idx);
    }
  } else { // Move
    uint64_t curr_state = getBoardKey(b);
    auto next_states = getNextStates(curr_state, b.pieces);
    if (!next_states.empty()) {
      std::uniform_int_distribution<size_t> dist_state(0,
                                                       next_states.size() - 1);
      uint64_t selected_state = next_states[dist_state(rng)];
      undo.type = MutationUndo::MOVE;
      undo.oldStateKey = curr_state;

      for (size_t i = 0; i < b.pieces.size(); i++) {
        b.setOccupied(b.pieces[i], false);
        b.pieces[i].position = (selected_state >> (i * 6)) & 0x3F;
        b.setOccupied(b.pieces[i], true);
      }
    }
  }
  return undo;
}

void undoMutation(Board &b, const MutationUndo &undo) {
  if (undo.type == MutationUndo::ADD) {
    b.removePiece(undo.pieceIdx);
  } else if (undo.type == MutationUndo::REMOVE) {
    b.pieces.insert(b.pieces.begin() + undo.pieceIdx, undo.oldPiece);
    b.setOccupied(undo.oldPiece, true);
  } else if (undo.type == MutationUndo::MOVE) {
    for (size_t i = 0; i < b.pieces.size(); i++) {
      b.setOccupied(b.pieces[i], false);
      b.pieces[i].position = (undo.oldStateKey >> (i * 6)) & 0x3F;
      b.setOccupied(b.pieces[i], true);
    }
  }
}

double getEnergy(const Board &b) {
  ExplorerResult res = exploreBoard(b);
  if (!res.solvable) {
    return 1.0;
  }
  return -res.maxMoves;
}

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

// Thread-Safe Level Generator Worker
GeneratedLevel generateSingleLevel(const LevelTask &task, std::mt19937 &rng) {
  while (true) {
    Board board;
    board.initEmpty();
    Piece key_piece{PRIMARY_PIECE_ROW * GRID_SIZE, PRIMARY_PIECE_SIZE, 0};
    board.addPiece(key_piece);

    std::uniform_int_distribution<int> dist_pieces(task.min_pieces,
                                                   task.max_pieces);
    int target_pieces = dist_pieces(rng);

    Board best_board = board;
    double max_temp = 20.0;
    double min_temp = 0.5;
    int steps = 150;
    double factor = -std::log(max_temp / min_temp);

    double curr_energy = getEnergy(board);
    double prev_energy = curr_energy;

    for (int step = 0; step < steps; step++) {
      double pct = (steps > 1) ? (double)step / (steps - 1) : 0;
      double temp = max_temp * std::exp(factor * pct);

      MutationUndo undo = mutateBoard(board, target_pieces, rng);
      if (undo.type == MutationUndo::NONE)
        continue;

      double energy = getEnergy(board);
      double change = energy - prev_energy;

      if (change > 0 &&
          std::exp(-change / temp) < ((double)rng() / rng.max())) {
        undoMutation(board, undo);
      } else {
        prev_energy = energy;
        if (energy < curr_energy) {
          curr_energy = energy;
          best_board = board;
        }
      }
    }

    ExplorerResult res = exploreBoard(best_board);
    if (res.solvable && res.maxMoves >= task.min_moves &&
        res.maxMoves <= task.max_moves) {
      sortPiecesCanonical(res.hardestBoard);
      return {res.hardestBoard, res.maxMoves};
    }
  }
}

int main() {
  std::cout << "=================================================="
            << std::endl;
  std::cout << "      Starting Level Generator  " << std::endl;
  std::cout << "=================================================="
            << std::endl;

  // Level configuration ensuring all targets have minimum moves > 10
  std::vector<LevelTask> tasks;
  for (int i = 1; i <= 100; i++) {
    if (i <= 25) {
      tasks.push_back({i, 5, 6, 11, 15}); // Moderate levels (11-15 moves)
    } else if (i <= 50) {
      tasks.push_back({i, 6, 7, 16, 20}); // Hard levels (16-20 moves)
    } else if (i <= 75) {
      tasks.push_back({i, 7, 8, 21, 25}); // Very Hard levels (21-25 moves)
    } else {
      tasks.push_back({i, 8, 9, 26, 40}); // Expert levels (26-40 moves)
    }
  }

  std::vector<GeneratedLevel> final_levels(100);
  std::atomic<int> next_task_idx(0);
  std::atomic<int> completed_tasks(0);
  std::mutex cout_mutex;

  unsigned int num_threads = std::thread::hardware_concurrency();
  if (num_threads == 0)
    num_threads = 4;
  std::cout << "Detected " << num_threads
            << " CPU cores. Running in parallel..." << std::endl;

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> workers;
  for (unsigned int t = 0; t < num_threads; t++) {
    workers.emplace_back([&, t]() {
      std::random_device rd;
      std::mt19937 rng(rd() + t);

      while (true) {
        int task_idx = next_task_idx.fetch_add(1);
        if (task_idx >= 100)
          break;

        GeneratedLevel lvl = generateSingleLevel(tasks[task_idx], rng);
        final_levels[task_idx] = lvl;

        int done = completed_tasks.fetch_add(1) + 1;
        {
          std::lock_guard<std::mutex> lock(cout_mutex);
          std::cout << "\rProgress: " << done
                    << "/100 levels successfully generated." << std::flush;
        }
      }
    });
  }

  for (auto &w : workers) {
    w.join();
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::high_resolution_clock::now() - start_time)
                     .count();

  std::cout << "\n\nAll levels generated in " << (double)elapsed / 1000.0
            << " seconds." << std::endl;
  std::cout << "Sorting levels by ascending difficulty (move counts)..."
            << std::endl;

  // Sort generated database by move counts
  std::sort(final_levels.begin(), final_levels.end(),
            [](const GeneratedLevel &a, const GeneratedLevel &b) {
              return a.moves < b.moves;
            });

  std::cout << "Writing static 'include/FKLevels.h' database..." << std::endl;

  std::ofstream out("include/FKLevels.h");
  out << "/*\n";
  out << " * FKLevels.h - Static Prebuilt Levels Header Database\n";
  out << " * Generated natively on PC with Simulated Annealing & BFS Solver.\n";
  out << " * Sorted in ascending order of optimal moves (difficulty).\n";
  out << " */\n\n";
  out << "#ifndef LEVELS_H\n";
  out << "#define LEVELS_H\n\n";
  out << "#include <Arduino.h>\n\n";
  out << "#ifndef MAX_FK_BLOCKS\n";
  out << "#define MAX_FK_BLOCKS 12\n";
  out << "#endif\n\n";
  out << "struct FKBlock {\n";
  out << "  int8_t x, y;\n";
  out << "  int8_t w, h;\n";
  out << "  bool isKey;\n";
  out << "  bool active;\n";
  out << "};\n\n";
  out << "struct PrebuiltLevel {\n";
  out << "  uint8_t blockCount;\n";
  out << "  uint8_t minMoves;\n";
  out << "  FKBlock blocks[MAX_FK_BLOCKS];\n";
  out << "};\n\n";
  out << "const PrebuiltLevel PREBUILT_LEVELS[100] PROGMEM = {\n";

  for (size_t idx = 0; idx < final_levels.size(); idx++) {
    const auto &lvl = final_levels[idx];
    out << "  {\n";
    out << "    " << lvl.board.pieces.size() << ", // Level " << idx + 1
        << " vehicle count\n";
    out << "    " << lvl.moves << ", // Optimal moves to solve\n";
    out << "    {\n";

    for (size_t p_idx = 0; p_idx < lvl.board.pieces.size(); p_idx++) {
      const auto &p = lvl.board.pieces[p_idx];
      int w = (p.orientation == 0) ? p.size : 1;
      int h = (p.orientation == 0) ? 1 : p.size;
      std::string is_key = (p_idx == 0) ? "true" : "false";
      int px = p.position % GRID_SIZE;
      int py = p.position / GRID_SIZE;

      out << "      {" << px << ", " << py << ", " << w << ", " << h << ", "
          << is_key << ", true}";
      if (p_idx < lvl.board.pieces.size() - 1 ||
          lvl.board.pieces.size() < MAX_FK_BLOCKS) {
        out << ",";
      }
      out << " // Car " << p_idx << "\n";
    }

    // Fill remaining inactive blocks up to MAX_FK_BLOCKS (12)
    for (int empty_idx = lvl.board.pieces.size(); empty_idx < MAX_FK_BLOCKS;
         empty_idx++) {
      out << "      {0, 0, 0, 0, false, false}";
      if (empty_idx < MAX_FK_BLOCKS - 1) {
        out << ",";
      }
      out << "\n";
    }

    out << "    }\n";
    if (idx < 99) {
      out << "  },\n";
    } else {
      out << "  }\n";
    }
  }
  out << "};\n\n";
  out << "#endif\n";

  out.close();
  std::cout << "Export complete! Done." << std::endl;
  return 0;
}
