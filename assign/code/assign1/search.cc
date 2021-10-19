#include <vector>
#include <iostream>
#include <set>
#include <queue>
#include "imdb.h"
#include "path.h"
using namespace std;

path getShortestPathBetweenActors(const string& actor1, const string& actor2, const imdb& db)
{
  set<film> visited_movies;
  set<string> visited_actors;
  queue<path> paths_queue;
  paths_queue.push(path(actor1));
  while(!paths_queue.empty())
  {
    auto pth = paths_queue.front();
    paths_queue.pop();
    const string& last_actor = pth.getLastPlayer();
    if(last_actor == actor2)
    {
      return pth;
    }else if(pth.getLength() >= 6)
    {
      return path(actor1);
    }
    vector<film> last_actor_movies{};
    db.getCredits(last_actor, last_actor_movies);
    for(const film& movie: last_actor_movies)
    {
      if(!visited_movies.contains(movie))
      {
        visited_movies.insert(movie);
        vector<string> movie_actors{};
        db.getCast(movie, movie_actors);
        for(const string& actor: movie_actors)
        {
          if(!visited_actors.contains(actor))
          {
            visited_actors.insert(actor);
            pth.addConnection(movie, actor);
            paths_queue.push(pth);
            pth.undoConnection();
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
