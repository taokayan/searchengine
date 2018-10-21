# webpage crawler & search engine
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

# Usage:

# Step 1 (Optional): Compile Your own WebCrawler & Search Engine:
Download SearchEngine & Multiplexer(light weight library). Open SearchEngine\SearchEngine.sln using Visual Studio 2017 and then build the whole project. You'll find SearchEngine\x64\Release\SearchEngine.exe will be successfully created if compilation success.

# Step 2: Initial Setup
In SearchEngine\x64\Release, create a folder called ```dbs```, then create a file called ```urls.txt``` inside ```dbs```. Open ```urls.txt```, enter one or more initial URLs line by line, for example: ```nesdev.com``` 

Run ```SearchEngine.exe```, you'll see a small menu like follows:
```
Last build time:Oct 21 2018 21:49:19
1. run URL crawling
2. run content shortening
3. start ranking downloaded content
4. search pages
101. exteral sort test
102. winHttp page download
103. database compress test
Please select:
```
Great! The program is started successfully. Type ```1``` then press enter to start crawling.

# Step 3: Web page crawling
In crawling mode, there're some parameter that need to be entered:
- ```number of threads```: This indicates number of con-current thread to crawl the internet. More threads will result in higher throughput but also result in high latency in slow network. Try to set it roughly as follows:
   - <100Mbps network or for testing purpose: 100
   - 200Mbps network: 200 - 300
   - 500Mbps network: 500 - 1000
   - 1Gbps network: 1000 or above

- ```max crawling URLs```: This indicates the maximum number of pages the application is going to download. Try to set it as follows:
   - 5GB free RAM:   100000000
   - 10GB free RAM:  200000000
   - 50GB free RAM: 1000000000
   



