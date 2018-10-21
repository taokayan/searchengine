# search engine
A light weight webpage crawler and search engine with the following advantages:

- Written purely in C++, no boost required, no 3rd party database required, run natively on everybody's Windows 10/7 x64.
- Lightning fast crawling speed (2000 pages/sec) with ordinary single i7 CPU, in a normal 7200rpm harddisk.
- Support HTTP & HTTPS webpages.
- Crawling with heuristic, avoid frequency http request to the same host.
- Support billions of pages, no theorectical limit.
- Ultra low memory usage: overall memory usage linear to 64MB / million pages, or 64GB / billion pages.
- Low harddisk usage: less than 50KB per page storage, including reverse indexing storage. 1 billion pages only need 50TB
- Optimized for harddisk: less than 50 harddisk access per second, no SSD required
- Optimized for reverse-indexing with in-place merge sort
- Optimized for searching: O(log N) hard-disk access for each key word
- Multi-language searching support: support all UTF-8 compatible searching characters. 

ref: http://dujiaen.blogspot.com/

