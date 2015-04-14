CCache(Common Cache) is a cache libary writing for the one who would like to cache data for frequent visiting.It caches data using mmap and makes you operating (insert, find, update, delete etc.) data very fast.
Unlike memcached, it is a static libary, not a server, and if you want to use it, you must write server by youself, ccache does not care the protocol between server and client, and you must write your function pointer to compare the key in cache when you want to operate it(see demo in the test/testcache.c).

# CommonCache Main feature #
  * using mmap to cache data in file, so it can be used in multi-thead and multi-process application.
  * support unfix size key node
  * support hash-rbtree and hash-list structure
  * use LRU algorithm to eliminate nodes when it is running out of its memory size
  * fast, 100W/read operation on fix-size key node in no more than 0.5 second

# CommonCache document #

Following is the Commoncache document:
  * [Desing And Implementation Of Ccache](http://code.google.com/p/commoncache/wiki/Design)
  * [a simple demo show how to use ccache](http://code.google.com/p/commoncache/wiki/demo)