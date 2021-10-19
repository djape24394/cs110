#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include "imdb.h"
using namespace std;

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";
imdb::imdb(const string& directory) {
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const {
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

imdb::~imdb() {
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

bool imdb::getCredits(const string& player, vector<film>& films) const { 
  films.clear();
  const int *countp = (const int *) actorFile;
  const int *begin = (const int *) actorFile + 1;
  const int *end = begin + *countp;
  const int *found = lower_bound(begin, end, player, [this](int offset, const string& player) {
      return getActorFromOffset(offset).compare(player) < 0;
  });
  
  if((found != end) && (getActorFromOffset(*found) != player))
  {
    return false;
  }

  auto [nof_movies, movies_begin] = getActorMoviesData(*found, player);
  const int *movies_end = movies_begin + nof_movies;
  std::transform(movies_begin, movies_end, std::back_inserter(films), [this](int movie_offset){
    return getMovieFromOffset(movie_offset);
  });

  return true;
}

bool imdb::getCast(const film& movie, vector<string>& players) const {
  players.clear();
  const int* p_count = (const int *) movieFile;
  const int* begin = (const int *) movieFile + 1;
  const int* end = begin + *p_count;
  const int* p_found_movie_offset = lower_bound(begin, end, movie, [this](int offset, const film& movie)
  {
    return getMovieFromOffset(offset) < movie;
  });

  if((p_found_movie_offset != end) && !(getMovieFromOffset(*p_found_movie_offset) == movie))
  {
    return false;
  }
  
  auto [nof_actors, actors_begin] = getMovieActorsData(*p_found_movie_offset, movie);
  const int *actors_end = actors_begin + nof_actors;
  std::transform(actors_begin, actors_end, std::back_inserter(players), [this](int actor_offset){
    return getActorFromOffset(actor_offset);
  });

  return true;
}

const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info) {
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info) {
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}

film imdb::getMovieFromOffset(int movie_offset)const
{
  const char *movie_location = (const char *)movieFile + movie_offset;
  film movie;
  movie.title = string(movie_location);
  movie.year = (int) *((const char *) movie_location + movie.title.size() + 1);
  return movie;
}

string imdb::getActorFromOffset(int actor_offset)const
{
  return string((const char *) actorFile + actor_offset);
}

std::pair<int, const int*> imdb::getActorMoviesData(int actor_offset, const string& actor)const
{
  const char *p_actor = (const char *)actorFile + actor_offset;
  int relative_offset = actor.size() + 1;

  return getArrayDataFromPointers(relative_offset, p_actor);
}

std::pair<int, const int *> imdb::getMovieActorsData(int movie_offset, const film& movie)const
{
  const char *p_movie = (const char *) movieFile + movie_offset;
  int relative_offset = movie.title.size() + 2; // characters + '\0' + byte for year
  
  return getArrayDataFromPointers(relative_offset, p_movie);
}

std::pair<int, const int*> imdb::getArrayDataFromPointers(int relative_offset, const char* p_data)const
{
  if(relative_offset % 2 != 0) relative_offset++;
  int nof_elements = *((short *)(p_data + relative_offset));
  relative_offset += 2;
  if(relative_offset % 4 != 0) relative_offset += 2;

  const int *array_begin = (const int *)(p_data + relative_offset);
  return {nof_elements, array_begin};
}
