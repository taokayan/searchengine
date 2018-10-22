# webpage crawler & search engine
A light weight webpage crawler and search engine with the following advantages:

- Written purely in C++, no boost required, no 3rd party database required, run natively on everybody's Windows 10/7 x64.
- Lightning fast crawling speed (2000 pages/sec) with ordinary single i7 CPU, in a normal 7200rpm harddisk.
- Support HTTP & HTTPS webpages.
- Crawling with heuristic, avoid frequent http requests to the same host.
- Support billions of pages, no theorectical limit.
- Ultra low memory usage: overall memory usage linear to 64MB / million pages, or 64GB / billion pages.
- Low harddisk usage: less than 50KB per page storage, including reverse indexing storage. 1 billion pages only need 50TB
- Optimized for harddisk: less than 50 harddisk access per second, no SSD required
- Optimized for reverse-indexing with in-place merge sort, O(log N) hard-disk access for each searching key word
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
   
You'll see the crawling stats from the console similar as follows:
```
hosts:10673(bad:25 ok:467) url:(contentDB:4727 processing+bad:484 ok:4727 pend:148134) conn:30ms recv:781ms parse:7ms sentBytes:90407 rcvdBytes:(http 17815927, https 391327938) compressed:47828150 idleThreads:0 freeRAM:12025MB
PendDBs:[    44     17     14     37     34      9     13     24     32     36     23     33     77     11     20     40     31     30     23     55    616     50     34     69     45     15     64     25     10     25     41     44     26     14     27     75     44     33     19     33     23     17     31      7    112     45     37     32     23     13     29     87     33    183     14    107    160     28     14     30     26     18     27    222    156     23     98     21     68    127     33      7     77    114     15    201     48     56     46     84     49     45     51    618     56     32     50     17     64     22     24      7     20     18     19     78     11     22     69    150 ]
lastURL:https://www.vox.com/policy-and-politics/2018/10/19/17999402/trump-montana-gop-guardian-bodyslam-violence-civility
```
the meanings for each numbers are:
- host: number of hosts requested so far, including bad hosts(no response) and good hosts
- url: 
   - contentDB: number of downloaded webpages in the database
   - processing+bad: number of downloading webpages, including time-outed webpages
   - pend: number of seens urls that are not yet downloaded
- conn: average connection latency in ms
- recv: average receiving latency
- parse: average webpage content parsing latency
- sendBytes: number of bytes sent to internet
- rcvdBytes: number of bytes received from internet, including http pages & https pages
- compressed: number of bytes after page compressio
- idelThread: number of idle crawling threads
- freeRAM: free system memory

Please notice that page downloading will run forever. To finish it manually, please type ```exit``` then enter, or kill the process.



