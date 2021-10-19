#include <vector>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "imdb.h"
#include "path.h"
using namespace std;

struct ConnectionHash
{
  std::size_t operator()(const path::connection& cn)const
  {
    const std::hash<std::string> hs; 
    return hs(cn.movie.title) ^ cn.movie.year ^ hs(cn.player);
  }
};


struct ConnectionEqual
{
  bool operator()(const path::connection& cn1, const path::connection& cn2)const
  {
    return cn1.movie == cn2.movie && cn1.player == cn2.player;
  }
};

struct FilmHash
{
  std::size_t operator()(const film& f)const
  {
    const std::hash<std::string> hs; 
    return hs(f.title) ^ f.year;
  }
};

path create_path(const string& actor1, path::connection &last_connection, unordered_map<path::connection, path::connection, ConnectionHash, ConnectionEqual> &parent)
{
  path pth(last_connection.player);
  
  while(last_connection.player != actor1)
  {
    path::connection next_connection = parent[last_connection];
    pth.addConnection(last_connection.movie, next_connection.player);
    last_connection = std::move(next_connection);
  }
  pth.reverse();
  return pth;
}

path getShortestPathBetweenActors(const string& actor1, const string& actor2, const imdb& db)
{
  unordered_set<film, FilmHash> visited_movies;
  unordered_set<string> visited_actors;
  unordered_map<path::connection, path::connection, ConnectionHash, ConnectionEqual> parent;
  queue<path::connection> paths_queue;
  paths_queue.push(path::connection(film(), actor1));
  visited_actors.insert(actor1);
  while(!paths_queue.empty())
  {
    auto q_conn = paths_queue.front();
    paths_queue.pop();
    if(q_conn.player == actor2)
    {
      return create_path(actor1, q_conn, parent);
    }
    vector<film> last_actor_movies{};
    db.getCredits(q_conn.player, last_actor_movies);
    for(const film& movie: last_actor_movies)
    {
      if(visited_movies.find(movie) == visited_movies.end())
      {
        visited_movies.insert(movie);
        vector<string> movie_actors{};
        db.getCast(movie, movie_actors);
        for(const string& actor: movie_actors)
        {
          if(visited_actors.find(actor) == visited_actors.end())
          {
            visited_actors.insert(actor);
            path::connection cn(movie, actor);
            paths_queue.push(cn);
            parent[cn] = q_conn;
          }
        }
      }
    }
  }
  return path(actor1);
}

int main(int argc, char *argv[]) {
  if(argc != 3) 
  {
    std::cout << "You must specify 2 actors!!\n";
    return 0;
  }
  string actor1(argv[1]);
  string actor2(argv[2]);
  imdb db(kIMDBDataDirectory);
  vector<film> actor1_movies;
  vector<film> actor2_movies;
  if(!db.getCredits(actor1, actor1_movies))
  {
    std::cout << "First specified actor doesn't exist in database!!\n";
    return 0;
  }
  if(!db.getCredits(actor2, actor2_movies))
  {
    std::cout << "Second specified actor doesn't exist in database!!\n";
    return 0;
  }
  if(actor1 == actor2)
  {
    std::cout << "Two actors names can not be the same!! \n";
    return 0;
  }
  string& a1 = (actor1_movies.size() <= actor2_movies.size()) ? actor1 : actor2;
  string& a2 = (actor1_movies.size() <= actor2_movies.size()) ? actor2 : actor1;
  bool flip = (actor1_movies.size() > actor2_movies.size());
  path pth = getShortestPathBetweenActors(a1, a2, db);
  if(flip) pth.reverse();
  if(!pth.getLength())
  {
    std::cout << "The path between the two actors doesn't exist!\n";
  }else
  {
    std::cout << pth;
  }
  
  return 0;
}
