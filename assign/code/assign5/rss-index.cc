/**
 * File: rss-index.cc
 * ------------------
 * Presents the implementation of the RSSIndex class, which is
 * little more than a glorified map.
 */

#include "rss-index.h"
#include "utils.h"

#include <algorithm>

using namespace std;

void RSSIndex::add(const Article& article, const vector<string>& words) {
  ArticleServerUrl article_server_url;
  article_server_url.url = getURLServer(article.url);
  article_server_url.title = article.title;

  auto words_cpy = words;
  sort(words_cpy.begin(), words_cpy.end());
  if(words_map.find(article_server_url) == words_map.end())
  {
    words_map[article_server_url] = std::move(words_cpy);
    lexic_smallest_url[article_server_url] = article.url;
  }else
  {
    vector<string> intersection_words;
    std::set_intersection(words_map[article_server_url].cbegin(), words_map[article_server_url].cend(), 
                          words_cpy.cbegin(), words_cpy.cend(), back_inserter(intersection_words));
    words_map[article_server_url] = intersection_words;
    if(article.url < lexic_smallest_url[article_server_url])
    {
      lexic_smallest_url[article_server_url] = article.url;
    }
  }

  // for(const string& word: words)
  // {
  //   index[word][article]++;
  // }
}

void RSSIndex::finalizeIndex()
{
  for(const auto& [article_server_url, words]: words_map)
  {
    Article article;
    article.url = lexic_smallest_url[article_server_url];
    article.title = article_server_url.title;
    for(const string& word: words)
    {
      index[word][article]++;
    }
  }
  // clear memory for lexic_smallest_url and words_map
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
