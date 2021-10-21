/**
 * File: rss-index.cc
 * ------------------
 * Presents the implementation of the RSSIndex class, which is
 * little more than a glorified map.
 */
#include <algorithm>
#include "rss-index.h"
#include "utils.h"

using namespace std;

void RSSIndex::add(const Article& article, const vector<string>& words) {
  
  pair<string, string> servUrlAndTitle{getURLServer(article.url), article.title};
  vector<string> words_cpy(words);
  sort(words_cpy.begin(), words_cpy.end());
  if(words_map.find(servUrlAndTitle) == words_map.end())
  {
    words_map[servUrlAndTitle] = std::move(words_cpy);
    lexic_smallest_url[servUrlAndTitle] = article.url;
  }else
  {
    vector<string> intersection_words;
    std::set_intersection(words_map[servUrlAndTitle].cbegin(), words_map[servUrlAndTitle].cend(), 
                          words_cpy.cbegin(), words_cpy.cend(), back_inserter(intersection_words));
    words_map[servUrlAndTitle] = intersection_words;
    if(article.url < lexic_smallest_url[servUrlAndTitle])
    {
      lexic_smallest_url[servUrlAndTitle] = article.url;
    } 
  }
}

void RSSIndex::finalizeIndex()
{
  for(const auto& [servUrlAndTitle, words]: words_map)
  {
    Article article;
    article.url = lexic_smallest_url[servUrlAndTitle];
    article.title = servUrlAndTitle.second;
    for(const string& word: words)
    {
      index[word][article]++;
    }
  }
  // // clear memory for lexic_smallest_url and words_map
  words_map.clear();
  lexic_smallest_url.clear();
}

static const vector<pair<Article, int> > emptyResult;
vector<pair<Article, int> > RSSIndex::getMatchingArticles(const string& word) const {
  auto indexFound = index.find(word);
  if (indexFound == index.end()) return emptyResult;
  const map<Article, int>& matches = indexFound->second;
  vector<pair<Article, int> > v;
  for (const pair<Article, int>& match: matches) v.push_back(match);
  sort(v.begin(), v.end(), [](const pair<Article, int>& one, 
                              const pair<Article, int>& two) {
   return one.second > two.second || (one.second == two.second && one.first < two.first);
  });
  return v;
}
