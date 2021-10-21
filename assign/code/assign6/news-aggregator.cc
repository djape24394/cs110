/**
 * File: news-aggregator.cc
 * --------------------------------
 * Presents the implementation of the NewsAggregator class.
 */

#include "news-aggregator.h"
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#include <unordered_set>
#include <thread>
// you will almost certainly need to add more system header includes

// I'm not giving away too much detail here by leaking the #includes below,
// which contribute to the official CS110 staff solution.
#include "rss-feed.h"
#include "rss-feed-list.h"
#include "html-document.h"
#include "html-document-exception.h"
#include "rss-feed-exception.h"
#include "rss-feed-list-exception.h"
#include "utils.h"
#include "ostreamlock.h"
#include "string-utils.h"
using namespace std;

/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Factory method that spends most of its energy parsing the argument vector
 * to decide what rss feed list to process and whether to print lots of
 * of logging information as it does so.
 */
static const string kDefaultRSSFeedListURL = "small-feed.xml";
NewsAggregator *NewsAggregator::createNewsAggregator(int argc, char *argv[]) {
  struct option options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"url", required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0},
  };
  
  string rssFeedListURI = kDefaultRSSFeedListURL;
  bool verbose = false;
  while (true) {
    int ch = getopt_long(argc, argv, "vqu:", options, NULL);
    if (ch == -1) break;
    switch (ch) {
    case 'v':
      verbose = true;
      break;
    case 'q':
      verbose = false;
      break;
    case 'u':
      rssFeedListURI = optarg;
      break;
    default:
      NewsAggregatorLog::printUsage("Unrecognized flag.", argv[0]);
    }
  }
  
  argc -= optind;
  if (argc > 0) NewsAggregatorLog::printUsage("Too many arguments.", argv[0]); 
  return new NewsAggregator(rssFeedListURI, verbose);
}

/**
 * Method: buildIndex
 * ------------------
 * Initalizex the XML parser, processes all feeds, and then
 * cleans up the parser.  The lion's share of the work is passed
 * on to processAllFeeds, which you will need to implement.
 */
void NewsAggregator::buildIndex() {
  if (built) return;
  built = true; // optimistically assume it'll all work out
  xmlInitParser();
  xmlInitializeCatalog();
  processAllFeeds();
  xmlCatalogCleanup();
  xmlCleanupParser();
}

/**
 * Method: queryIndex
 * ------------------
 * Interacts with the user via a custom command line, allowing
 * the user to surface all of the news articles that contains a particular
 * search term.
 */
void NewsAggregator::queryIndex() const {
  static const size_t kMaxMatchesToShow = 15;
  while (true) {
    cout << "Enter a search term [or just hit <enter> to quit]: ";
    string response;
    getline(cin, response);
    response = trim(response);
    if (response.empty()) break;
    const vector<pair<Article, int> >& matches = index.getMatchingArticles(response);
    if (matches.empty()) {
      cout << "Ah, we didn't find the term \"" << response << "\". Try again." << endl;
    } else {
      cout << "That term appears in " << matches.size() << " article"
           << (matches.size() == 1 ? "" : "s") << ".  ";
      if (matches.size() > kMaxMatchesToShow)
        cout << "Here are the top " << kMaxMatchesToShow << " of them:" << endl;
      else if (matches.size() > 1)
        cout << "Here they are:" << endl;
      else
        cout << "Here it is:" << endl;
      size_t count = 0;
      for (const pair<Article, int>& match: matches) {
        if (count == kMaxMatchesToShow) break;
        count++;
        string title = match.first.title;
        if (shouldTruncate(title)) title = truncate(title);
        string url = match.first.url;
        if (shouldTruncate(url)) url = truncate(url);
        string times = match.second == 1 ? "time" : "times";
        cout << "  " << setw(2) << setfill(' ') << count << ".) "
             << "\"" << title << "\" [appears " << match.second << " " << times << "]." << endl;
        cout << "       \"" << url << "\"" << endl;
      }
    }
  }
}

/**
 * Private Constructor: NewsAggregator
 * -----------------------------------
 * Self-explanatory.  You may need to add a few lines of code to
 * initialize any additional fields you add to the private section
 * of the class definition.
 */
NewsAggregator::NewsAggregator(const string& rssFeedListURI, bool verbose): 
  log(verbose), rssFeedListURI(rssFeedListURI), built(false), tp_rss(3), tp_articles(20){}

/**
 * Private Method: processAllFeeds
 * -------------------------------
 * Downloads and parses the encapsulated RSSFeedList, which itself
 * leads to RSSFeeds, which themsleves lead to HTMLDocuemnts, which
 * can be collectively parsed for their tokens to build a huge RSSIndex.
 * 
 * The vast majority of your Assignment 5 work has you implement this
 * method using multithreading while respecting the imposed constraints
 * outlined in the spec.
 */

void NewsAggregator::processAllFeeds() 
{
  RSSFeedList rssFeedList(rssFeedListURI);
  try
  {
    rssFeedList.parse();
  }
  catch(const RSSFeedListException& e)
  {
    log.noteFullRSSFeedListDownloadFailureAndExit(rssFeedListURI);
  }
  log.noteFullRSSFeedListDownloadEnd();
  // where the keys are the links, and the values are the titles
  const std::map<url, title>& feeds = rssFeedList.getFeeds();
  
  unordered_set<string> visitedURLs;
  mutex urls_lock;

  for(const auto& [feedURL, feedTitle]: feeds)
  {
    bool visited = false;
    {
      scoped_lock<mutex> lg(urls_lock);
      if(visitedURLs.find(feedURL) == visitedURLs.end())
      {
        visitedURLs.insert(feedURL);
      }else
      {
        visited = true;
      }
    }

    if(!visited)
    {
      tp_rss.schedule([this, feedURL, &visitedURLs, &urls_lock](){
        processFeed(feedURL, visitedURLs, urls_lock);
      });
    }
  }
  tp_rss.wait();
  tp_articles.wait();
  index.finalizeIndex();
  log.noteAllRSSFeedsDownloadEnd();
}

void NewsAggregator::processFeed(const string& feedURL, unordered_set<string> &visitedURLs, mutex &urls_lock)
{
  try
  {
    RSSFeed rssFeed(feedURL);
    log.noteSingleFeedDownloadBeginning(feedURL);
    rssFeed.parse();
    const std::vector<Article>& rssFeedArticles = rssFeed.getArticles();
    vector<thread> threads;
    for(const Article& article: rssFeedArticles)
    {
      bool visited = false;
      {
        lock_guard<mutex> lg(urls_lock);
        if(visitedURLs.find(article.url) == visitedURLs.end())
        {
          visitedURLs.insert(article.url);
        }
        else
        {
          visited = true;
        }
      }

      if(!visited)
      {
        tp_articles.schedule([this, article](){
          processArticle(article);
        });
      }
    }
    for(thread& thrd: threads)
    {
      thrd.join();
    }
    log.noteSingleFeedDownloadEnd(feedURL);
  }
  catch(const RSSFeedException& e)
  {
    log.noteSingleFeedDownloadFailure(feedURL);
  }
} 


void NewsAggregator::processArticle(const Article& article)
{
  try{
    log.noteSingleArticleDownloadBeginning(article);
    HTMLDocument htmlDocument(article.url);
    htmlDocument.parse();
    const std::vector<std::string>& articleTokens = htmlDocument.getTokens();
    {
      std::lock_guard<mutex> lg(indexLock);
      index.add(article, articleTokens);
    }
  }
  catch(const HTMLDocumentException& e)
  {
    log.noteSingleArticleDownloadFailure(article);
  }
}
