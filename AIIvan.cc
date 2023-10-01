#include "Player.hh"

/**
 * Write the name of your player and save this file
 * with the same name and .cc extension.
 */
#define PLAYER_NAME Ivan


struct PLAYER_NAME : public Player {

  /**
   * Factory: returns a new instance of this class.
   * Do not modify this function.
   */
  static Player* factory () {
    return new PLAYER_NAME;
  }

  // Quadrant the player spawns in:     1   2
  //                                    3   4
  int my_quadrant;

  // Center position of the board
  const Pos center_pos = Pos(board_rows()/2, board_cols()/2);

  // Center position of the quadrant the player spawns in
  Pos quadrant_center_pos; 

  // The program will consider an ant a2 being near of an ant a1 if a2 is inside a square of (2*near_value + 1)*(2*near_value + 1),
  // which center is a1.pos
  const int near_value = 3;

  // Set that contains all positions used by my ants in the current round
  set<Pos> used_positions;

  // Stores the queen's position
  Pos queen_pos;

  // Stores a bonus being chased by the queen
  Pos queens_ambition;

  // Map that contains a path to follow in order to reach a certain position for an ant which id is the key of the element
  // The first element of the map is the ant id, ant the second is another map whose first element is the actual position and
  // the second element is the previous position of it
  map<int, map<Pos, Pos>> path;

  // Map that stores all the living workers in the end of a certain turn, just in order to keep clean "path" and "target"
  map<int, bool> living_workers;

  // Map that stores all the living soldiers in the end of a certain turn, just in order to keep clean "path" and "target"
  map<int, bool> living_soldiers;

  // Map that stores a bonus position as a key and the ant's id is chasing the bonus. Note that one specific bonus can be chased by one
  // ant at the same time
  map<Pos, int> target;

  // Computes the quadrant the player's ants start in, and the center position of it
  void compute_quadrant() {
    int queen_id = queens(me())[0];
    Ant queen = ant(queen_id);
    Pos p = queen.pos;                                          //              1   2
    if (p.i < center_pos.i and p.j < center_pos.j) {            // Quadrants:   3   4
      my_quadrant = 1;
    } else if (p.i < center_pos.i and p.j > center_pos.j) {
      my_quadrant = 2;
    } else if (p.i > center_pos.i and p.j < center_pos.j) {
      my_quadrant = 3;
    } else if (p.i > center_pos.i and p.j > center_pos.j) {
      my_quadrant = 4;
    }

    Pos q;
    switch (my_quadrant) {
      case 1:
        q.i = center_pos.i/2;
        q.j = center_pos.j/2;
        break;

      case 2:
        q.i = center_pos.i/2;
        q.j = center_pos.j + center_pos.j/4;
        break;

      case 3:
        q.i = center_pos.i + center_pos.i/4;
        q.j = center_pos.j/2;
        break;

      case 4:
        q.i = center_pos.i + center_pos.i/4;
        q.j = center_pos.j + center_pos.j/4;
        break;
    }
    quadrant_center_pos = q;
  }

  // Returns the first bonus found in a star area with center at "center", or (-1, -1) otherwise
  Pos search_for_a_bonus(int ant_id, const Pos& center) {
    queue<Pos> pos_to_visit;                                    // Queue containing all positions that we have discovered but not visited yet
    set<Pos> pos_used;                                          // Set containing all positions visited
    pos_used.insert(center);
    pos_to_visit.push(center);

    while (not pos_to_visit.empty()) {
      Pos p = pos_to_visit.front();
      pos_to_visit.pop();
      vector<int> v = random_permutation(4);
      for (int dir = 0; dir < 4; ++dir) {
        Dir direction = Dir(v[dir]);
        if (pos_ok(p + direction) and cell(p + direction).id == -1 and cell(p + direction).bonus != None and target.find(p + direction) == target.end()) {
          target.insert({p + direction, ant_id});
          return p + direction;
        }
        if (pos_ok(p + direction) and cell(p + direction).type != Water and pos_used.find(p + direction) == pos_used.end()) {
          pos_used.insert(p + direction);
          pos_to_visit.push(p + direction);
        }
      }
    }

    return Pos(-1, -1);    
  }

  // Returns true if the queen is near the position p(near = it's inside a 3x3 square, whose center is p)
  bool queen_is_near(const Pos& p) {
    int qi = queen_pos.i;
    int qj = queen_pos.j;
    return qi >= p.i-near_value and qi <= p.i+near_value and qj >= p.j-near_value and qj <= p.j+near_value;
  }

  // Computes the shortest path from src to dst
  void bfs(int ant_id, const Pos& src, const Pos& dst) {
    queue<pair<Pos, int>> Q;                                              // Position p and distance between p and src
    Q.push({src, 0});
    set<Pos> visited_positions;                                           // Set that contains all visited positions
    map<Pos, Pos> previous_positions;                                     // Map that contains a certain position and its previous position (at distance 1)
    previous_positions.insert({src, Pos(-1, -1)});                        // src is the first position

    while (not Q.empty()) {
      Pos actual_pos = Q.front().first;
      int dist_src_pos = Q.front().second;
      Q.pop();
      if (actual_pos == dst) {
        path.insert({ant_id, previous_positions});
        return;
      }

      if (visited_positions.find(actual_pos) == visited_positions.end()) {
        visited_positions.insert(actual_pos);
        vector<int> random_dirs = random_permutation(4);
        for (int i = 0; i < 4; ++i) {                                       // For every direction
          Pos new_pos = actual_pos + Dir(random_dirs[i]); 
          if (pos_ok(new_pos) and cell(new_pos).type != Water) {
            Q.push({new_pos, dist_src_pos + 1});
            if (previous_positions.find(new_pos) == previous_positions.end()) {   // Previous position of new_pos is actual_pos
              previous_positions.insert({new_pos, actual_pos});
            }
          }
        }
      }
    }

    path.insert({ant_id, previous_positions});
  }

  // Returns the direction an ant must follow to reach dst from src(they are at distance 1)
  Dir get_direction(const Pos& src, const Pos& dst) {
    if (src.i == dst.i) return src.j < dst.j ? Right : Left; 
    return src.i < dst.i ? Down : Up;
  }

  // Moves an ant with id ant_id to the next position in order to reach its destiny
  void continue_your_trip(int ant_id, const Pos& current, const Pos& dst) {
    Pos q = path.find(ant_id)->second.find(dst)->second;
    while (current != path.find(ant_id)->second.find(q)->second and path.find(ant_id)->second.find(q)->second != q) {
      q = path.find(ant_id)->second.find(q)->second;
    }
    
    if (pos_ok(q) and cell(q).type != Water and cell(q).id == -1 and used_positions.find(q) == used_positions.end()) {
      move(ant_id, get_direction(current, q));
      used_positions.insert(q);
    }
  }

  // Returns the type of ant the queen can produce, or a type Queen ant if it cannot lay any egg
  AntType which_egg(const vector<int>& queen_reserve) {
    if (queen_reserve[Carbohydrate] >= needed(Worker, Carbohydrate) and 
        queen_reserve[Lipid] >= needed(Worker, Lipid) and 
        queen_reserve[Protein] >= needed(Worker, Protein)) {
      return Worker;
    }
    if (queen_reserve[Carbohydrate] >= needed(Soldier, Carbohydrate) and 
        queen_reserve[Lipid] >= needed(Soldier, Lipid) and 
        queen_reserve[Protein] >= needed(Soldier, Protein)) {
      return Soldier;
    }
    return Queen;
  }

  // Tries to lay an egg
  void try_to_lay(int queen_id, const AntType& a, bool& queens_action_done) {
    for (int i = 0; i < 4; ++i) {
      Pos p = queen_pos + Dir(i);
      Cell c = cell(p);
      if (pos_ok(p) and c.type != Water and c.id == -1 and used_positions.find(p) == used_positions.end()) {
        used_positions.insert(p);
        queens_action_done = true;
        lay(queen_id, Dir(i), a);
        return;
      }
    }
  }

  // Returns the first worker rival ant found in a star area with center at "center", or (-1, -1) otherwise
  Pos search_for_a_victim(const Pos& center, int radius) {
    queue<Pos> pos_to_visit;                                    // Queue containing all positions that we have discovered but not visited yet
    set<Pos> pos_used;                                          // Set containing all positions visited
    pos_used.insert(center);
    pos_to_visit.push(center);

    while (not pos_to_visit.empty()) {
      Pos p = pos_to_visit.front();
      pos_to_visit.pop();
      vector<int> perm_dirs = random_permutation(4);
      for (int dir = 0; dir < 4; ++dir) {
        Dir direction = Dir(perm_dirs[dir]);
        Cell c = cell(p + direction);
        if (pos_ok(p + direction) and c.type != Water and c.id != -1 and ant(c.id).player != me() and (ant(c.id).type == Worker or ant(c.id).type == Soldier)) {
          return p + direction;
        }
        if (pos_ok(p + direction) and c.type != Water and pos_used.find(p + direction) == pos_used.end()) {
          pos_used.insert(p + direction);
          pos_to_visit.push(p + direction);
        }
      }
    }

    return Pos(-1, -1);
  }

  // Erases dead ants' information
  void erase_dead_ants() {
    map<int, bool>::iterator it = living_workers.begin(), it2;
    while (it != living_workers.end()) {
      if (not it->second) {
        it2 = it;
        ++it;
        path.erase(it2->first);
        living_workers.erase(it2->first);
      }
      else ++it;
    }
    it = living_soldiers.begin();
    while (it != living_soldiers.end()) {
      if (not it->second) {
        it2 = it;
        ++it;
        path.erase(it2->first);
        living_soldiers.erase(it2->first);
      }
      else ++it;
    }
  }

  // Moves ant with id ant_id to a valid and empty random direcion
  void move_randomly(int ant_id, const Pos& p) {
    vector<int> random_dirs = random_permutation(4);
    for (int i = 0; i < 4; ++i) {
      Dir d = Dir(random_dirs[i]);
      Pos q = p + d;
      Cell c = cell(q);
      if (pos_ok(p) and c.type != Water and c.id == -1 and used_positions.find(q) == used_positions.end()) {
        used_positions.insert(q);
        move(ant_id, d);
        return;
      }
    }
  }

  // Computes which quadrant p is in
  int quadrant(const Pos& p) {
    if (p.i < center_pos.i and p.j < center_pos.j) {
      return 1;
    } else if (p.i < center_pos.i and p.j > center_pos.j) {
      return 2;
    } else if (p.i > center_pos.i and p.j < center_pos.j) {
      return 3;
    } 
    return 4;
  }

  // Moves the ant with id ant_id to another quadrant(des_quadrant)
  void go_away(const Pos& p, int dest_quadrant, int ant_id) {
    Dir d;
    switch(dest_quadrant) {
      case 1:
        d = get_direction(quadrant_center_pos, Pos(center_pos.i/2, center_pos.j/2));
        break;

      case 2:
        d = get_direction(quadrant_center_pos, Pos(center_pos.i/2, 3*center_pos.j/4));
        break;

      case 3:
        d = get_direction(quadrant_center_pos, Pos(3*center_pos.i/4, center_pos.j/2));
        break;

      default:
        d = get_direction(quadrant_center_pos, Pos(3*center_pos.i/4, 3*center_pos.i/4));
        break;
    }

    if (pos_ok(p + d) and cell(p + d).type != Water and cell(p + d).id == -1 and used_positions.find(p + d) == used_positions.end()) {
      used_positions.insert(p + d);
      move(ant_id, d);
    }
  }

  // Returns a random quadrant between the quadrants q has next to(diagonal quadrant discarted)s
  int adjacent_quadrant(int q) {
    int a = random(0, 1);
    switch (q) {
      case 1:
        return a == 0 ? 2 : 3;
        break;

      case 2:
        return a == 0 ? 1: 4;
        break;

      case 3:
        return a == 0 ? 1 : 4;
        break;
    }
    return a == 0 ? 2 : 3;
  }

  // Returns the first rival ant found in a star area with center at "center", or (-1, -1) otherwise
  Pos search_for_an_enemy(const Pos& center, int radius) {
    queue<Pos> pos_to_visit;                                    // Queue containing all positions that we have discovered but not visited yet
    set<Pos> pos_used;                                          // Set containing all positions visited
    pos_used.insert(center);
    pos_to_visit.push(center);
    int i = 0;

    while (i < radius) {
      Pos p = pos_to_visit.front();
      pos_to_visit.pop();
      vector<int> perm_dirs = random_permutation(4);
      for (int dir = 0; dir < 4; ++dir) {
        Dir direction = Dir(perm_dirs[dir]);
        Cell c = cell(p + direction);
        if (pos_ok(p + direction) and c.type != Water and c.id != -1 and ant(c.id).player != me() and (ant(c.id).type == Worker or ant(c.id).type == Soldier)) {
          return p + direction;
        }
        if (pos_ok(p + direction) and c.type != Water and pos_used.find(p + direction) == pos_used.end()) {
          pos_used.insert(p + direction);
          pos_to_visit.push(p + direction);
        }
      }
      ++i;
    }

    return Pos(-1, -1);
  }

  // Checks wether there's an enemy close to p
  bool enemy_close(const Pos& p) {
    Pos min;
    min.i = p.i - near_value;
    min.j = p.j - near_value;
    if (min.j < 0) min.j = 0;
    if (min.i < 0) min.i = 0;

    Pos max;
    max.i = p.i + near_value;
    max.j = p.j + near_value;
    if (max.i >= board_rows()) max.i = board_rows() - 1;
    if (max.j >= board_cols()) max.j = board_cols() - 1;

    for (int i = min.i; i < 2*near_value + 1; ++i) {
      for (int j = min.j; j < 2*near_value + 1; ++j) {
        Pos r = Pos(i, j);
        if (r != p and (cell(r).type != Water and cell(r).id != -1) and ant(cell(r).id).player != me()) return true;
      }
    }

    return false;
  }

  /**
   * Play method, invoked once per each round.
   */
  virtual void play () {
    used_positions.clear();
    living_soldiers.clear();
    living_workers.clear();
    target.clear();

    if (round() == 1) {
      int id_queen = queens(me())[0];
      Ant queen = ant(id_queen);
      queen_pos = queen.pos;
    }
    
    vector<int> my_soldier_ids = soldiers(me());
    vector<int> my_worker_ids = workers(me());

   //cerr << "------------------------------------------QUEEN------------------------------------------" << endl;

    // Command the queen
    if (round() % queen_period() == 0) {
      int queen_id = queens(me())[0];
      Ant queen = ant(queen_id);
      Pos p = queen.pos;

      vector<int> v = random_permutation(4);
      bool queens_action_done = false;

      if (not queens_action_done and my_soldier_ids.size() + my_worker_ids.size() >= 2) {     // To avoid an aggressive behaviour if the queen is alone
        for (int i = 0; i < 4; ++i) {                       // Search for a close enemy ant
          Dir d = Dir(v[i]);
          Cell c = cell(p + d);
          if (pos_ok(p + d) and c.type != Water and c.id != -1 and ant(c.id).player != me() and (ant(c.id).type == Worker or ant(c.id).type == Soldier) and used_positions.find(p + d) == used_positions.end()) {
            if (c.id != -1) {
              used_positions.insert(p + d);
              queens_action_done = true;
              move(queen_id, d);
              break;
            }
          } else if (used_positions.find(p + d) != used_positions.end()) queens_action_done = true;
        }
      }

      if (not queens_action_done) {
        for (int i = 0; i < 4; ++i) {                       // Search for a close bonus
          Dir d = Dir(v[i]);
          if (pos_ok(p + d) and cell(p + d).type != Water and cell(p + d).bonus != None and used_positions.find(p + d) == used_positions.end()) {
            if (cell(p + d).id == -1) {
              used_positions.insert(p + d);
              queens_action_done = true;
              path.erase(queen_id);
              move(queen_id, d);
              break;
            }
          } else if (used_positions.find(p + d) != used_positions.end()) queens_action_done = true;
        }
      }

      AntType a = which_egg(queen.reserve);
      if (a != Queen and not enemy_close(p)) {
        try_to_lay(queen_id, a, queens_action_done);
      }

      if (not queens_action_done) {
        if (path.find(queen_id) == path.end()) {                  // Searches for an enemy soldier or worker
          queens_ambition = search_for_an_enemy(p, 3);
          if (pos_ok(queens_ambition)) {
            path.erase(queen_id);
            bfs(queen_id, p, queens_ambition);
            if (path.find(queen_id) != path.end()) {
              continue_your_trip(queen_id, p, queens_ambition);
              queens_action_done = true;
            }
          }
        }
        if (1) {                  // Searches for a bonus
          queens_ambition = search_for_a_bonus(queen_id, p);
          if (pos_ok(queens_ambition)) {
            path.erase(queen_id);
            bfs(queen_id, p, queens_ambition);
            if (path.find(queen_id) != path.end()) {
              continue_your_trip(queen_id, p, queens_ambition);
              queens_action_done = true;
            }
          }
        } 
        else {
          continue_your_trip(queen_id, p, queens_ambition);
        }

        if (not queens_action_done) {                              // Move to a random direction
          for (int i = 0; i < 4; ++i) {
            Dir d = Dir(v[i]);
            if (pos_ok(p + d) and cell(p + d).type != Water and cell(p + d).id == -1 and used_positions.find(p + d) == used_positions.end()) {
              used_positions.insert(p + d);
              queens_action_done = true;
              move(queen_id, d);
              break;
            }
          }
        }
      }

      queen_pos = p;
    }

   //cerr << "------------------------------------------WORKERS------------------------------------------" << endl;

    // Command my workers in random order.
    if (1) {
      vector<int> workers_perm = random_permutation(my_worker_ids.size());
      for (int k = 0; k < int(workers_perm.size()); ++k) {
        int worker_id = my_worker_ids[workers_perm[k]];
        living_workers.insert({worker_id, true});
        Ant worker = ant(worker_id);
        Pos p = worker.pos;
        bool close_to_queen = queen_is_near(p);

        if (worker.bonus == None) {
          if (cell(p).bonus != None and not close_to_queen) {
            take(worker_id);
          } 
          else if (cell(p).bonus != None and close_to_queen) {
            move_randomly(worker_id, p);
          }
          else if (cell(p).bonus == None and close_to_queen) {
            move_randomly(worker_id, p);
          }
          else {
            Pos bonus = search_for_a_bonus(worker_id, p);
            if (pos_ok(bonus)) {
              if (path.find(worker_id) != path.end()) path.erase(worker_id);
              bfs(worker_id, p, bonus);
              continue_your_trip(worker_id, p, bonus);
            }
          }
        }
        else if (worker.bonus != None) {
          if (not close_to_queen) {
            if (path.find(worker_id) != path.end()) path.erase(worker_id);
            bfs(worker_id, p, queen_pos);
            continue_your_trip(worker_id, p, queen_pos);
          }
          else if (close_to_queen) {
            leave(worker_id);
          }
        }        



        
        
      }
    }
   
   //cerr << "------------------------------------------SOLDIERS------------------------------------------" << endl;

    // Command my soldiers in random order
    if (1) {
      vector<int> soldiers_perm = random_permutation(my_soldier_ids.size());
      for (int i = 0; i < int(my_soldier_ids.size()); ++i) {
        int soldier_id = my_soldier_ids[soldiers_perm[i]];
        living_soldiers.insert({soldier_id, true});
        Ant soldier = ant(soldier_id);
        Pos p = soldier.pos;
        bool soldier_moved = false;
        int quad = quadrant(p);
        
        for (int dir = 0; dir < 4; ++dir) {               // Kills any ant (rivals only) if it's close to the soldier 
          Dir d = Dir(dir);
          Pos q = p + d;
          Cell c = cell(q);
          if (pos_ok(q) and c.type != Water and c.id != -1 and ant(c.id).player != me() and (ant(c.id).type == Worker or ant(c.id).type == Soldier)) {
            if ((quad != my_quadrant and ant(c.id).type == Worker) or used_positions.find(q) == used_positions.end()) {
              used_positions.insert(q);
              move(soldier_id, d);
              soldier_moved = true;
              if (path.find(soldier_id) != path.end()) path.erase(soldier_id);
              break;
            }
          }
        }
        
        if (round() <= 30) {
          bfs(soldier_id, p, center_pos);
          if (path.find(soldier_id) != path.end()) {
            continue_your_trip(soldier_id, p, center_pos);
          }
        }

        if (quad != my_quadrant) {
          if (not soldier_moved and round() >= 20) {
            if (round() == 31) path.erase(soldier_id);
            if (path.find(soldier_id) == path.end()) {
              Pos victim = search_for_a_victim(p, 15);
              if (pos_ok(victim)) {
                bfs(soldier_id, p, victim);
                if (path.find(soldier_id) != path.end()) {
                  continue_your_trip(soldier_id, p, victim);
                }
                soldier_moved = true;
              }
            }
          }

        
          if (not soldier_moved) {
            move_randomly(soldier_id, p);
            soldier_moved = true;
          }
        }
        else go_away(p, adjacent_quadrant(quad), soldier_id);
      }
    }
    
    erase_dead_ants();

  }

};


/**
 * Do not modify the following line.
 */
RegisterPlayer(PLAYER_NAME);
