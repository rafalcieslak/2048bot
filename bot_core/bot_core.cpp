#include "Python.h"
#include <cstdio>
#include <cmath>
/*
  The class representing a board state. It is in fact just a 64 bit number plus several useful functions
  implementing common operations.
*/
typedef unsigned short pos;
typedef unsigned long long stateval;
class State{
public:
    State() = delete;
    State(stateval q) : value(q) {}
    State(const State& other) = default;
    stateval get_val() const {return value;}
    int get_tile_at(pos p) const {return (value>>((15-p)*4)) & 0x0f;}
    int get_tile_at(pos x, pos y) const {return get_tile_at(y*4+x);}
    void move_up()   { for(int y = 0; y < 4; y++) for(int x = 0; x <  4; x++) move_tile_up(x,y)   ; }
    void move_left() { for(int x = 0; x < 4; x++) for(int y = 0; y <  4; y++) move_tile_left(x,y) ; }
    void move_down() { for(int y = 3; y >= 0; y--) for(int x = 3; x >= 0; x--) move_tile_down(x,y) ; }
    void move_right(){ for(int x = 3; x >= 0; x--) for(int y = 3; y >= 0; y--) move_tile_right(x,y); }
    int get_free_tiles() const {int r = 0; for(pos p = 0; p < 16; p++) if(get_tile_at(p)==0) r++; return r;}
    bool operator==(const State& other){return value == other.value;}
    bool operator!=(const State& other){return value != other.value;}
    void set_at(pos p, int v) { 
        stateval offset = ((15-p)*4);
        value &= ~((stateval)0x0f<<offset);
        value |= ((v & (stateval)0x0f)<<offset);
    }
    void set_at(pos x, pos y, int v) {set_at(y*4+x,v);}
private:
    stateval value;
    void move_tile_up(pos x, pos y){
        int v = get_tile_at(x,y); if(v == 0) return;
        for(int i = y-1; i >= -1; i--){
            if(i == -1)         { set_at(x,y,0); set_at(x,i+1,v); return;} // push to borders
            if(get_tile_at(x,i) == v){ set_at(x,y,0); set_at(x,i,v+1); return;} // or merge with same value
            if(get_tile_at(x,i) != 0){ set_at(x,y,0); set_at(x,i+1,v); return;} // or push to tile
        }
    }
    void move_tile_down(pos x, pos y){
        int v = get_tile_at(x,y); if(v == 0) return;
        for(int i = y+1; i <= 4; i++){
            if(i == 4)          { set_at(x,y,0); set_at(x,i-1,v); return;} // push to borders
            if(get_tile_at(x,i) == v){ set_at(x,y,0); set_at(x,i,v+1); return;} // or merge with same value
            if(get_tile_at(x,i) != 0){ set_at(x,y,0); set_at(x,i-1,v); return;} // or push to tile
        }
    }
    void move_tile_left(pos x, pos y){
        int v = get_tile_at(x,y); if(v == 0) return;
        for(int i = x-1; i >= -1; i--){
            if(i == -1)         { set_at(x,y,0); set_at(i+1,y,v); return;} // push to borders
            if(get_tile_at(i,y) == v){ set_at(x,y,0); set_at(i,y,v+1); return;} // or merge with same value
            if(get_tile_at(i,y) != 0){ set_at(x,y,0); set_at(i+1,y,v); return;} // or push to tile
        }
    }
    void move_tile_right(pos x, pos y){
        int v = get_tile_at(x,y); if(v == 0) return;
        for(int i = x+1; i <= 4; i++){
            if(i == 4)          { set_at(x,y,0); set_at(i-1,y,v); return;} // push to borders
            if(get_tile_at(i,y) == v){ set_at(x,y,0); set_at(i,y,v+1); return;} // or merge with same value
            if(get_tile_at(i,y) != 0){ set_at(x,y,0); set_at(i-1,y,v); return;} // or push to tile
        }
    }
};

// Forward declarations
double evaluate(const State& s, bool verbose = false);
int run(const State& s, int depth);


// =========================================
// These functions implement a Python module.
extern "C" PyObject *
bot_evaluate(PyObject *self, PyObject *args){
	unsigned long long s;
    if (!PyArg_ParseTuple(args, "K", &s))
        return NULL;
    double result = evaluate(State(s));
    return Py_BuildValue("d", result);
}

extern "C" PyObject *
bot_run(PyObject *self, PyObject *args)
{
	unsigned long long s;
    int depth;
    if (!PyArg_ParseTuple(args, "Ki", &s, &depth))
        return NULL;
    int q = run(State(s),depth);
    return Py_BuildValue("i", q);
}

static PyMethodDef BotMethods[] =
{
     {"run", bot_run, METH_VARARGS, "Run search."},
     {"evaluate", bot_evaluate, METH_VARARGS, "Evaluate a state."},
     {NULL, NULL, 0, NULL}
};

extern "C" PyMODINIT_FUNC
initbot_core(void)
{
    (void) Py_InitModule("bot_core", BotMethods);
}
// ==========================================

// The actual search logic begins here.

/* This is the function that calculates a score for a game state, so that the best one (highest score) can
   be chosen. This is the place for heurestics.
   Currently this is a simple formula that takes into account only the following factors: number of free
   tiles, and the size of the highest and the second highest tile number.
*/
double evaluate(const State& s, bool verbose){
    int free_tiles = s.get_free_tiles();
    int max_tile = 0, second_max_tile = 0;
    for(int i = 0; i < 16; i++) {
        if(s.get_tile_at(i) > max_tile){ second_max_tile = max_tile; max_tile = s.get_tile_at(i); }
        else if(s.get_tile_at(i) > second_max_tile) {second_max_tile = s.get_tile_at(i);}
    }
    // The Magical Formula!
    double score = double(free_tiles) + pow(2,(double)(max_tile)+0.5) + pow(2,0.8*((double)(second_max_tile))+0.5);
    if(verbose ){
        // Debug info.
        printf("ft: %d, mt: %d, smt: %d\n", free_tiles, max_tile, second_max_tile);
        printf("Evaluating %llx.\n",s.get_val());
        printf("s = %lf\n", score);
    }
    return score;
}

/*
  The search is done as a tree, where vertices represent game states.
  run()ning a node will recursivelly build the tree up to given depth,
  calculating expeced value as the score and storing it in .result.

  It is probably not efficient to implement this as a set of self-containing classes,
  but this way resource management is very simple, and the search concept is clearly
  visible, and thus it is easy to modify it.
*/
class SearchTreeNode{
public:
    SearchTreeNode(const State& orig) : origstate(orig) {}
    const State& origstate;
    double result = -9999;
    int best_move = -1;
    virtual void run(int depth) = 0; //pure virtual
};
class RandomTreeNode : public SearchTreeNode{
public:
    RandomTreeNode(const State& orig) : SearchTreeNode(orig) {}
    void run(int depth);
};
class PlayerTreeNode : public SearchTreeNode{
public:
    PlayerTreeNode(const State& orig) : SearchTreeNode(orig) {}
    void run(int depth);
};

/* This node checks all possible player moves, and finds the one of the best value.
   It assumes that the player (the bot) will select the best moves in the future,
   so it produces the maximum of all recursivelly calculated scores.
*/
void PlayerTreeNode::run(int depth){
        // No recursive searches if depth is 0.
        if(depth == 0) {result = evaluate(origstate); return;}
        // Create 4 new game states, let each represent one of four possible moves.
        State T[4] = {State(origstate),State(origstate),State(origstate),State(origstate)};
        T[0].move_up(); T[1].move_down(); T[2].move_left(); T[3].move_right();
        // Recursivelly search all these states, but ignore the ones that are the same as current (such move is invalid).
        RandomTreeNode N[4] = {RandomTreeNode(T[0]),RandomTreeNode(T[1]),RandomTreeNode(T[2]),RandomTreeNode(T[3])};
        for(int i = 0; i <= 3; i++){
            if(T[i]!=origstate) N[i].run( depth-1 );
        }
        // Find which move provides the highest score.
        short best_i = -1; double best_v = -9999;
        for(int i = 0; i <= 3; i++){ if(N[i].result > best_v) {best_v = N[i].result; best_i = i;}}
        result = best_v; best_move = best_i;
        if(best_i == -1) result = -9999; // If no move can be done, this is an endgame!

}
/* This node checks for all possible random spawns and calculates expected value.
   It applies probabilities to all states, and returns (as a result) a weighed
   average score.

   Another possible implementation of this node would always assume the pesymistic case, 
   and return the lowest possible score. This way the bot would aviod risk at all cost.
*/
void RandomTreeNode::run(int depth){
    // No recursive searches if depth is 0.
    if(depth == 0) {result = evaluate(origstate); return;}
    double sum = 0.0;
    int count = 0;
    // For each position on the game board
    for(int i = 0; i < 16; i++){
        if(origstate.get_tile_at(i) != 0) continue; // Ignore tiles where a tile cannot spawn.
        // Consider having a 2 or a 4 spawned here.
        State newstate2(origstate);
        State newstate4(origstate);
        newstate2.set_at(i,1);
        newstate4.set_at(i,2);
        // Run search recursivelly.
        PlayerTreeNode node2(newstate2); node2.run(depth-1);
        PlayerTreeNode node4(newstate4); node4.run(depth-1);
        // Apply probabilites as weights.
        sum += 0.9 * node2.result;
        sum += 0.1 * node4.result;
        count += 1;
    }
    // Calculate total.
    result = sum/count;
}

// A helper function that sets the root of the search tree, and initiates recursive search.
int run(const State& state, int depth){
    PlayerTreeNode root(state);
    root.run(depth);
    printf("Expected score in %d moves: %lf\n", depth, root.result);
    return root.best_move;
}
