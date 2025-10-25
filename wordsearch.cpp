/*
=====================================================================
WORD SEARCH PUZZLE GENERATOR
---------------------------------------------------------------------
Generates a word search grid that maximizes overlap between words.
=====================================================================
*/

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <ctime>
#include <set>
#include <chrono>
using namespace std;

/*---------------------------------------------------------------
  GLOBAL VARIABLES & CONSTANTS
---------------------------------------------------------------*/
int GRID_ROWS = 0, GRID_COLS = 0;
int MAX_RUNTIME_MS = 2000;
chrono::steady_clock::time_point solver_start_time;

// All 8 directions (horizontal, vertical, diagonal)
static const vector<pair<int,int>> DIRECTIONS = {
    {0,1}, {0,-1}, {1,0}, {-1,0}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
};

/*---------------------------------------------------------------
  DATA STRUCTURES
---------------------------------------------------------------*/
struct WordPlacement {
    string word;
    int row, col;
    int delta_row, delta_col;
};

struct PuzzleResult {
    vector<string> grid;
    vector<WordPlacement> placements;
    vector<string> placed_words;
    vector<string> unplaced_words;
    int num_placed = 0;
    int total_overlap_score = 0;
};

/*---------------------------------------------------------------
  HELPER FUNCTIONS
---------------------------------------------------------------*/

// Convert string to uppercase alphabetic only
string normalizeWord(const string &input) {
    string clean;
    for(char ch : input)
        if(isalpha((unsigned char)ch))
            clean.push_back(toupper((unsigned char)ch));
    return clean;
}

// Check if a cell is within grid boundaries
bool isInBounds(int r, int c) {
    return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}

// Check if solver exceeded runtime limit
bool hasTimedOut() {
    auto now = chrono::steady_clock::now();
    int elapsed_ms = chrono::duration_cast<chrono::milliseconds>(now - solver_start_time).count();
    return elapsed_ms >= MAX_RUNTIME_MS;
}

/*---------------------------------------------------------------
  GRID VALIDATION & PLACEMENT
---------------------------------------------------------------*/

// Validate word placement and count overlaps
bool canPlaceWord(const vector<string>& grid, const string &word, int r, int c, int dr, int dc, int &overlap_count) {
    overlap_count = 0;
    for(char ch : word) {
        if(!isInBounds(r, c)) return false;
        char existing = grid[r][c];
        if(existing != '.' && existing != ch) return false;
        if(existing == ch) overlap_count++;
        r += dr; c += dc;
    }
    return true;
}

// Write word into grid
void placeWord(vector<string>& grid, const string &word, int r, int c, int dr, int dc) {
    for(char ch : word) {
        grid[r][c] = ch;
        r += dr; c += dc;
    }
}

/*---------------------------------------------------------------
  OVERLAP SCORING
---------------------------------------------------------------*/

// Compute total overlap score for all placed words
int calculateOverlapScore(const vector<WordPlacement>& placements) {
    if(placements.empty()) return 0;
    vector<vector<int>> overlap_map(GRID_ROWS, vector<int>(GRID_COLS, 0));

    for(const auto &p : placements) {
        int rr = p.row, cc = p.col;
        for(char ch : p.word) {
            if(isInBounds(rr, cc)) overlap_map[rr][cc]++;
            rr += p.delta_row; cc += p.delta_col;
        }
    }

    int overlap_score = 0;
    for(int r = 0; r < GRID_ROWS; r++)
        for(int c = 0; c < GRID_COLS; c++)
            if(overlap_map[r][c] > 1)
                overlap_score += (overlap_map[r][c] - 1);
    return overlap_score;
}

/*---------------------------------------------------------------
  RECURSIVE BACKTRACKING ALGORITHM
---------------------------------------------------------------*/
void solvePuzzleRecursively(const vector<string>& words,
                            const vector<int>& word_order,
                            int current_index,
                            vector<string>& grid,
                            vector<WordPlacement>& current_placements,
                            vector<bool>& used_flags,
                            PuzzleResult &best_result) {

    if(hasTimedOut()) return;

    int placed_count = current_placements.size();
    int current_overlap = calculateOverlapScore(current_placements);

    // Update best solution
    if(placed_count > best_result.num_placed || 
       (placed_count == best_result.num_placed && current_overlap > best_result.total_overlap_score)) {
        best_result.num_placed = placed_count;
        best_result.total_overlap_score = current_overlap;
        best_result.grid = grid;
        best_result.placements = current_placements;
    }

    if(current_index >= (int)word_order.size()) return;

    // Stop if no chance to improve best
    int remaining = word_order.size() - current_index;
    if(placed_count + remaining <= best_result.num_placed) return;

    int word_index = word_order[current_index];
    const string &current_word = words[word_index];

    struct Candidate { int r, c, dr, dc, overlap; };
    vector<Candidate> candidates;

    // Generate valid placement options
    for(int r = 0; r < GRID_ROWS; r++)
        for(int c = 0; c < GRID_COLS; c++)
            for(auto dir : DIRECTIONS) {
                int overlap_val;
                if(canPlaceWord(grid, current_word, r, c, dir.first, dir.second, overlap_val))
                    candidates.push_back({r, c, dir.first, dir.second, overlap_val});
            }

    // Sort by overlap and centrality
    sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if(a.overlap != b.overlap) return a.overlap > b.overlap;
        int dist_a = abs(a.r - GRID_ROWS/2) + abs(a.c - GRID_COLS/2);
        int dist_b = abs(b.r - GRID_ROWS/2) + abs(b.c - GRID_COLS/2);
        return dist_a < dist_b;
    });

    // Recursive placement attempts
    for(const auto &cand : candidates) {
        if(hasTimedOut()) return;
        vector<string> new_grid = grid;
        placeWord(new_grid, current_word, cand.r, cand.c, cand.dr, cand.dc);
        current_placements.push_back({current_word, cand.r, cand.c, cand.dr, cand.dc});
        used_flags[word_index] = true;

        solvePuzzleRecursively(words, word_order, current_index + 1, new_grid, current_placements, used_flags, best_result);

        used_flags[word_index] = false;
        current_placements.pop_back();
    }

    // Optionally skip this word
    if(!hasTimedOut())
        solvePuzzleRecursively(words, word_order, current_index + 1, grid, current_placements, used_flags, best_result);
}

/*---------------------------------------------------------------
  PUZZLE SOLVER ENTRY FUNCTION
---------------------------------------------------------------*/
PuzzleResult generateWordSearch(const vector<string>& words,
                                const vector<bool>& required_flags,
                                int rows, int cols, int runtime_ms) {
    GRID_ROWS = rows;
    GRID_COLS = cols;
    MAX_RUNTIME_MS = runtime_ms;
    solver_start_time = chrono::steady_clock::now();
    vector<string> blank_grid(rows, string(cols, '.'));

    // Determine optimal word order
    vector<int> word_order;
    for(int pass = 0; pass < 2; ++pass) {
        vector<pair<int,int>> tmp;
        for(int i = 0; i < (int)words.size(); ++i)
            if((pass == 0) == required_flags[i])
                tmp.push_back({(int)words[i].size(), i});
        sort(tmp.begin(), tmp.end(), greater<>());
        for(auto &p : tmp) word_order.push_back(p.second);
    }

    vector<bool> used_flags(words.size(), false);
    vector<WordPlacement> current_placements;
    PuzzleResult best_result;

    solvePuzzleRecursively(words, word_order, 0, blank_grid, current_placements, used_flags, best_result);

    // Identify which words were placed
    set<string> placed_set;
    for(auto &p : best_result.placements) placed_set.insert(p.word);
    for(auto &w : words)
        (placed_set.count(w) ? best_result.placed_words : best_result.unplaced_words).push_back(w);

    // Fill remaining cells randomly
    mt19937_64 rng((uint64_t)chrono::steady_clock::now().time_since_epoch().count());
    uniform_int_distribution<int> dist(0, 25);
    for(int r = 0; r < rows; r++)
        for(int c = 0; c < cols; c++)
            if(best_result.grid[r][c] == '.')
                best_result.grid[r][c] = char('A' + dist(rng));

    return best_result;
}

/*---------------------------------------------------------------
  MAIN FUNCTION
---------------------------------------------------------------*/
int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int cli_rows = 0, cli_cols = 0;
    for(int i = 1; i < argc; i++) {
        string arg = argv[i];
        if(arg.rfind("--rows=", 0) == 0) cli_rows = stoi(arg.substr(7));
        else if(arg.rfind("--cols=", 0) == 0) cli_cols = stoi(arg.substr(7));
        else if(arg.rfind("--timems=", 0) == 0) MAX_RUNTIME_MS = stoi(arg.substr(9));
    }

    vector<string> input_lines;
    string line;
    while(getline(cin, line)) {
        size_t a = line.find_first_not_of(" \t");
        size_t b = line.find_last_not_of(" \t");
        if(a == string::npos) continue;
        input_lines.push_back(line.substr(a, b - a + 1));
    }

    if(input_lines.empty()) {
        cerr << "Provide words via stdin (one per line). Prefix * for must-include words.\n";
        return 1;
    }

    vector<string> words;
    vector<bool> required_flags;
    int max_word_len = 0;

    for(auto &raw : input_lines) {
        bool required = false;
        string s = raw;
        if(!s.empty() && s[0] == '*') { required = true; s = s.substr(1); }
        string normalized = normalizeWord(s);
        if(normalized.empty()) continue;
        words.push_back(normalized);
        required_flags.push_back(required);
        max_word_len = max(max_word_len, (int)normalized.size());
    }

    if(words.empty()) {
        cerr << "No valid words found after normalization.\n";
        return 1;
    }

    int rows = cli_rows, cols = cli_cols;
    if(rows <= 0 || cols <= 0) {
        int total_letters = 0;
        for(auto &w : words) total_letters += (int)w.size();
        int estimated = max(max_word_len, (int)ceil(sqrt((double)total_letters)) + 2);
        rows = cols = max(estimated, 10);
    }

    PuzzleResult result = generateWordSearch(words, required_flags, rows, cols, MAX_RUNTIME_MS);

    // Output as JSON
    cout << "{\n";
    cout << "\"rows\": " << rows << ",\n";
    cout << "\"cols\": " << cols << ",\n";
    cout << "\"grid\": [\n";
    for(int r = 0; r < rows; r++)
        cout << "\"" << result.grid[r] << "\"" << (r + 1 < rows ? "," : "") << "\n";
    cout << "],\n\"placements\": [\n";
    for(size_t i = 0; i < result.placements.size(); ++i) {
        auto &p = result.placements[i];
        cout << "{\"word\":\"" << p.word << "\",\"row\":" << p.row
             << ",\"col\":" << p.col << ",\"dr\":" << p.delta_row << ",\"dc\":" << p.delta_col << "}"
             << (i + 1 < result.placements.size() ? "," : "") << "\n";
    }
    cout << "],\n\"placed_words\": [";
    for(size_t i = 0; i < result.placed_words.size(); ++i)
        cout << "\"" << result.placed_words[i] << "\"" << (i + 1 < result.placed_words.size() ? "," : "");
    cout << "],\n\"unplaced_words\": [";
    for(size_t i = 0; i < result.unplaced_words.size(); ++i)
        cout << "\"" << result.unplaced_words[i] << "\"" << (i + 1 < result.unplaced_words.size() ? "," : "");
    cout << "]\n}\n";

    return 0;
}
