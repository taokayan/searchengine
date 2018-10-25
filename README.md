# webpage crawler & search engine
A light weight webpage crawler and search engine with the following advantages:

- Written purely in C++, no boost required, no 3rd party database required, run natively on everybody's Windows 10/7 x64.
- Lightning fast crawling speed (2000 pages/sec) with ordinary single i7 CPU, in a normal 7200rpm harddisk.
- Support HTTP & HTTPS webpages.
- Crawling with heuristic, avoid frequent http requests to the same host.
- Support billions of pages, no theorectical limit.
- Ultra low memory usage: overall memory usage under 48MB / million pages, or 48GB / billion pages.
- Low harddisk usage: less than 50KB per page storage, including reverse indexing storage. 1 billion pages only need 50TB
- Optimized for harddisk: less than 50 harddisk access per second, no SSD required
- Optimized for reverse-indexing with in-place merge sort, O(log N) hard-disk access for each searching key word
- Multi-language searching support: support all UTF-8 compatible searching characters, especially Chinese.

detailed technical reference: http://dujiaen.blogspot.com/

# Usage:

# 1 (Optional): Compile Your own WebCrawler & Search Engine:
Download SearchEngine & Multiplexer(light weight library). Open SearchEngine\SearchEngine.sln using Visual Studio 2017 and then build the whole project. You'll find SearchEngine\x64\Release\SearchEngine.exe will be successfully created if compilation success.

# 2: Initial Setup
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

# 3: Web page crawling
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


# 4: Page ranking, word extraction and reverse indexing
If you have downloaded enough web pages at previous step, then it's excited to go forward. Restart the process then select ```3. start ranking downloaded content``` this time to start processing all downloaded pages!

parameters:
- ```# of iteration```: number of iteration to run the page ranking algorithm. That is, start from equal weight for every pages, then contribute to each linking pages. You can try 5.
- ```Override maxDBIndex```: if you only want to process the first N pages, enter your number N. Otherwise, enter 0 to including all downloaded pages.
- ```Max word per page```: maximum number of extracted words per page. Normally I'll use 2000.

Great, at this point you'll see the following tasks are performed:
- link extraction
- page ranking
- word extraction and reverse indexing, this might take long time to finish
- merge sort for all index, this might take long time to finish

The process will exit after merge sort finished, for example:
```
1. run URL crawling
2. run content shortening
3. start ranking downloaded content
4. search pages
101. exteral sort test
102. winHttp page download
103. database compress test
Please select:3
# of iterations:5
# downloaded pages = 40118, maxDBIndex = 40118
Override maxDBIndex:0
Max Words per Page:2000
urlMD5 count = 40118, key2rank count = 40118
Use the existing a pending rank file(pendingRank)?n
Link extract:95.32% #ok = 34187(cur:1769/s avg:1699/s jobqueue:4049), #failed:0, #link:3458711, #oklink:642586
Ranking iter 0: 100.00% processed 736053 total 736054 ok 736053 failed 0
Ranking iter 1: 100.00% processed 736053 total 736054 ok 736053 failed 0
Ranking iter 2: 100.00% processed 736053 total 736054 ok 736053 failed 0
Ranking iter 3: 100.00% processed 736053 total 736054 ok 736053 failed 0
Ranking iter 4: 100.00% processed 736053 total 736054 ok 736053 failed 0
maxRank 100.000000 minRank 0.151795 avgRank 0.664481
Ranking finished. Reverse Indexing starts...
#pages 36021(89.79% 3442/s) words:21494113 non-En-Words:1946721 maxWords:2000 smallWords:10938436 inQueue:4093 outQueue:0
Word/Pharse extraction completed, closing database...
Reverse Indexing, merge sort begins...
Exsort:flush dbs\finaldict_0, nThreads 4, sortTime 6375ms, nItems 23816896, nflushed 23816896
Exsort dbs\finaldict OK, 23816896 items, time 6172ms
main word/phrase dictionary built.
Exsort:flush dbs\finaldictSmall_0, nThreads 4, sortTime 3047ms, nItems 12159812, nflushed 12159812
Exsort dbs\finaldictSmall OK, 12159812 items, time 1613ms
small word dictionary built.

Reverse Indexing finished!!! time:29070ms
```

# 5: Search pages by key word
Congratulation, you can now use your search engine to search! Restart the process and select ```4. search pages``` to start searching. 

Note: Please type lower case letter only for English. Also you can use Chinese, Japanese or other languages. Use space to separate each keyword, and use ```"``` to quote the keyword that contain space. For example:

the result of keyword ```iphone```, reverse ordered by page rankings:
```
Please input search item:iphone
smallDict: 4888429-4889935(1506)
27992:1.122e+00 [1418(r:1.12e+00) ] [Technology, gaming and science news from Belfast Telegraph - BelfastTelegraph.co.uk] [https://www.belfasttelegraph.co.uk/business/technology]
25441:1.370e+00 [22(r:1.37e+00) ] [Inside Frieze London 2018, via Five iPhone Camera Rolls | AnOther] [www.anothermag.com/art-photography/11232/inside-frieze-london-2018-via-five-iphone-camera-rolls]
2208:1.422e+00 [159(r:1.42e+00) ] [Wolfram|Alpha: Mobile &amp; Tablet Apps] [products.wolframalpha.com/mobile/?source=nav#wapowered]
(+2 pages for products.wolframalpha.com)
23905:1.654e+00 [548(r:1.65e+00) ] [Yahoo Tech - Technology News and Reviews] [https://www.yahoo.com/tech]
34433:1.845e+00 [8(r:1.85e+00) ] [iPhone XS vs CINEMA Camera - Can you spot the DIFFERENCE? - YouTube] [https://www.youtube.com/watch?v=k2CJoR3F2t0]
37800:1.948e+00 [827(r:1.95e+00) ] [ABC | FAQ &amp; Support - ABC.com] [https://abc.go.com/faq]
32358:3.552e+00 [337(r:3.55e+00) ] [Support Home Page] [https://bostonglobe.custhelp.com/app/home]
23916:7.188e+00 [9(r:7.19e+00) ] [Iphone Xs Max | Latest News on Iphone Xs Max | Breaking Stories and Opinion Articles - Firstpost] [https://www.firstpost.com/tag/iphone-xs-max]
(+13 pages for www.firstpost.com)
28030:8.759e+00 [24(r:8.76e+00) ] [Digital Lifestyle News: Microsoft's Answer To iPhone; Babelgum Review] [https://web.archive.org/web/20110406103451/http://www.readwriteweb.com/archives/microsoft_answer_to_iph
one.php]
14405:1.495e+01 [441(r:1.50e+01) ] [1.1.1.1 — the Internet’s Fastest, Privacy-First DNS Resolver] [https://1.1.1.1]
2567:2.570e+01 [60(r:2.57e+01) ] [Best Smart Home Devices for 2018 - CNET] [https://www.cnet.com/topics/smart-home/best-smart-home-devices]
(+22 pages for www.cnet.com)
28276:3.325e+01 [22(r:3.33e+01) ] [The iPhone headphone dongle is Apple’s top accessory at Best Buy - The Verge] [https://www.theverge.com/platform/amp/circuitbreaker/2018/8/24/17779680/iphone-headphone-dongle
-most-popular-apple-accessory-best-buy]
(+2 pages for www.theverge.com)
19988:2.780e+02 [15(r:2.78e+02) ] [Hachi.tech | Buy iPhone XS products in Singapore] [https://www.hachi.tech/apple/iphone-m/iphone-m-iphone-xs]
(+3 pages for www.hachi.tech)
[iphone]:1506 utf8:69 70 68 6f 6e 65
Final:1506, read time:0ms, join/sort time:0ms, marshall time:31ms
```

The result of ```hong kong weather```:
```
Please input search item:hong kong weather
smallDict: 4534895-4536943(2048)
smallDict: 5200119-5202060(1941)
smallDict: 9897161-9898689(1528)
37684:2.999e-08 [86(r:1.22e-02) 87(r:1.21e-02)(c:1) 847(r:5.68e-03)(c:760) ] [DAKGALBI Paik&#39;s Pan Sizzling Korean Chicken - YouTube] [https://www.youtube.com/watch?v=X9UJDQVjVsQ&amp;index=78&amp;list=PURFj4
Yj1nKhgrT-_8AUsaDg]
1235:4.869e-08 [2033(r:6.62e-03) 2034(r:6.61e-03)(c:1) 341(r:3.55e-02)(c:1693) ] [Model 3 | Tesla] [https://www.tesla.com/model3]
872:8.885e-08 [907(r:2.01e-02) 908(r:2.01e-02)(c:1) 27(r:4.86e-02)(c:881) ] [The Latest US and World News - USATODAY.com] [https://www.usatoday.com/news]
40005:1.450e-07 [467(r:5.83e-02) 468(r:5.82e-02)(c:1) 1182(r:2.85e-02)(c:714) ] [Carte de Fenwick Island - H?tels et activités sur le plan de Fenwick Island - TripAdvisor] [https://fr.tripadvisor.be/LocalMaps-g
488164-Fenwick_Island-Area.html]
(+2 pages for fr.tripadvisor.be)
33313:3.097e-07 [401(r:1.24e-01) 402(r:1.24e-01)(c:1) 1115(r:6.10e-02)(c:713) ] [Mapa de Fenwick Island - Hoteles, hostales y atracciones en Fenwick Island en el mapa - TripAdvisor] [https://www.tripadvisor.com
.pe/LocalMaps-g488164-Fenwick_Island-Area.html]
(+2 pages for www.tripadvisor.com.pe)
18299:3.469e-07 [616(r:1.39e-01) 617(r:1.39e-01)(c:1) 1330(r:6.83e-02)(c:713) ] [Mapa de Galveston - Hoteles y atracciones en el mapa de Galveston - TripAdvisor] [https://www.tripadvisor.es/LocalMaps-g55879-Gal
veston-Area.html]
(+4 pages for www.tripadvisor.es)
770:7.178e-07 [2739(r:8.62e-02) 2740(r:8.61e-02)(c:1) 487(r:5.46e-01)(c:2253) ] [Office of the Secretary | Department of Commerce] [https://www.commerce.gov/doc/office-secretary]
(+4 pages for www.commerce.gov)
12156:1.057e-06 [179(r:3.37e-02) 180(r:3.36e-02)(c:1) 16(r:9.90e-01)(c:164) ] [China's winter weather threatens food supplies - Wikinews, the free news source] [https://en.wikinews.org/wiki/China%27s_winter_wea
ther_threatens_food_supplies]
5096:1.066e-06 [193(r:1.74e-03) 194(r:1.74e-03)(c:1) 115(r:4.71e-04)(c:79) ] [Gabriela Hearst Spring 2019 Ready-to-Wear Collection - Vogue] [https://www.vogue.com/fashion-shows/spring-2019-ready-to-wear/gabriel
a-hearst]
17847:1.272e-06 [12(r:6.36e-01) 13(r:6.35e-01)(c:1) 1011(r:5.89e-04)(c:998) ] [Hong Kong Travel Guide on TripAdvisor] [https://en.tripadvisor.com.hk/Travel_Guide-g294217-Hong_Kong.html]
33205:1.277e-06 [12(r:6.39e-01) 13(r:6.38e-01)(c:1) 1015(r:5.89e-04)(c:1002) ] [Hong Kong Travel Guide on TripAdvisor] [https://www.tripadvisor.co.uk/Travel_Guide-g294217-Hong_Kong.html]
23473:2.647e-06 [124(r:3.96e-02) 125(r:3.96e-02)(c:1) 22(r:4.39e-02)(c:103) ] [Home | Yahoo Answers] [https://answers.yahoo.com]
(+22 pages for answers.yahoo.com)
126:2.876e-06 [373(r:1.31e+00) 374(r:1.31e+00)(c:1) 633(r:2.55e-01)(c:259) ] [Wikipedia, the free encyclopedia] [https://en.wikipedia.org/wiki/Main_Page]
15105:4.280e-06 [12(r:2.14e+00) 13(r:2.14e+00)(c:1) 1075(r:7.40e-03)(c:1062) ] [Hong Kong Forum, Travel Discussion for Hong Kong, China - TripAdvisor] [https://www.tripadvisor.com.au/ShowForum-g294217-i1496-Hon
g_Kong.html]
(+3 pages for www.tripadvisor.com.au)
27745:7.062e-06 [696(r:2.84e+00) 697(r:2.84e+00)(c:1) 1413(r:1.39e+00)(c:716) ] [Hotéis da área de Belo Horizonte - Mapa de hotéis perto de Belo Horizonte - TripAdvisor] [https://www.tripadvisor.com.br/LocalMap
s-g303374-Belo_Horizonte-Area.html]
(+2 pages for www.tripadvisor.com.br)
4414:1.078e-05 [31(r:6.93e-03) 32(r:6.93e-03)(c:1) 102(r:1.61e-03)(c:70) ] [Seattle, Washington, Travel Guide & Tips - Condé Nast Traveler] [https://www.cntraveler.com/destinations/seattle]
1837:5.398e-05 [840(r:3.09e-03) 841(r:3.09e-03)(c:1) 791(r:3.25e-03)(c:50) ] [#uxjobs hashtag on Twitter] [https://twitter.com/hashtag/uxjobs?src=hash]
23124:1.145e+00 [8(r:3.82e-01) 9(r:3.82e-01)(c:1) 10(r:3.81e-01)(c:1) ] [Kowloon, Hong Kong - Weather Forecasts | Maps | News - Yahoo Weather] [https://www.yahoo.com/news/weather]
[hong]:2048 [kong]:1941 [weather]:1528 utf8:68 6f 6e 67 20 6b 6f 6e 67 20 77 65 61 74 68 65 72
Final:77, read time:16ms, join/sort time:0ms, marshall time:15ms
```

The result of ```香港```:
```
Please input search item:香港
smallDict: 12006865-12007879(1014)
1337:1.737e-02 [102(r:1.74e-02) ] [登入 - Google 帳戶] [https://myaccount.google.com/?utm_source=OGB&amp;utm_medium=app]
31221:1.817e-02 [26(r:1.82e-02) ] [- 雅虎香港 搜尋結果] [https://hk.promotions.yahoo.com/ysm/combinedsearch/?fr=slot-hotsearch-ysm&amp;type=slotting_ysm214z&amp;p=cctv&amp;kg=0&amp;tag=]
800:2.047e-02 [92(r:2.05e-02) ] [YouTube] [https://youtu.be]
28730:3.729e-02 [98(r:3.73e-02) ] [Regulatory Compliance  - PCI DSS, HIPAA | Trend Micro] [https://www.trendmicro.com/en_us/business/capabilities/solutions-for/compliance.html]
(+3 pages for www.trendmicro.com)
27041:5.152e-02 [13(r:5.15e-02) ] [香港 10  大最佳五星級酒店 - TripAdvisor] [https://www.tripadvisor.com.hk/Hotels-g294217-zfc5-Hong_Kong-Hotels.html]
28134:6.230e-02 [13(r:6.23e-02) ] [香港 10  家最佳 3 星級飯店 - TripAdvisor] [https://www.tripadvisor.com.tw/Hotels-g294217-zfc3-Hong_Kong-Hotels.html]
(+8 pages for www.tripadvisor.com.tw)
30144:6.361e-02 [478(r:6.36e-02) ] [File:NYC subway-4D.svg – Travel guide at Wikivoyage] [https://en.wikivoyage.org/wiki/File:NYC_subway-4D.svg]
11483:7.889e-02 [620(r:7.89e-02) ] [ProPublica] [https://m.facebook.com/propublica/?locale2=zh_HK]
22745:1.167e-01 [8(r:1.17e-01) ] [Yahoo雅虎香港] [https://www.yahoo.com]
1753:1.565e-01 [445(r:1.56e-01) ] [Supported languages - translatewiki.net] [https://translatewiki.net/wiki/Special:SupportedLanguages]
(+18 pages for translatewiki.net)
28377:3.115e-01 [497(r:3.11e-01) ] [Google Earth] [https://www.google.com/intl/hu/earth]
(+5 pages for www.google.com)
27582:1.223e+00 [105(r:1.22e+00) ] [Olav3D Tutorials - YouTube] [https://www.youtube.com/channel/UCD0GTet7PkOuVH26nrfeNfA]
(+8 pages for www.youtube.com)
25882:7.833e+00 [17(r:7.83e+00) ] [擴展事業的 CRM 及雲端運算 - Salesforce 香港] [https://www.salesforce.com]
[香港]:1014 utf8:e9 a6 99 e6 b8 af
Final:1014, read time:0ms, join/sort time:0ms, marshall time:94ms
```

- Detail result explaination:
```
23124:1.145e+00 [8(r:3.82e-01) 9(r:3.82e-01)(c:1) 10(r:3.81e-01)(c:1) ] [Kowloon, Hong Kong - Weather Forecasts | Maps | News - Yahoo Weather] [https://www.yahoo.com/news/weather]
[hong]:2048 [kong]:1941 [weather]:1528 utf8:68 6f 6e 67 20 6b 6f 6e 67 20 77 65 61 74 68 65 72
Final:77, read time:16ms, join/sort time:0ms, marshall time:15ms
```
- 1st number ```23124```: the index of the page in content DB
- ```1.145e+00```: overall ranking for this page under this search
- ```8(r:3.82e-01)```: the location of ```hong``` is 8 in this page, and the ranking of this word in this page is 3.82e-01
- ```9(r:3.82e-01)(c:1)```: location of ```kong``` is 9 in this page, and the ranking of this word is 3.82e-01, and the location difference between ```hong``` and ```kong``` is 1
- ```10(r:3.81e-01)(c:1)```: location of ```weather``` is 10 in this page, ranking 3.81e-01, location difference between ```kong``` and ```weather``` is 1
- ```Kowloon, Hong Kong - Weather Forecasts | Maps | News - Yahoo Weather``` page title
- ```https://www.yahoo.com/news/weather``` page link
- ```[hong]:2048```: there are 2048 pages containing keyword ```hong```
- ```Final:77```: there are 77 pages containing ```hong``` and ```kong``` and ```weather```.



Enjoy your life with your own search engine!!! Please help to improve your own search engine.

If you wish to donate, here is my addresses:

BTC: 1NLhfXbjZX18r9vucVhSDCKVkeD1mPA2N2

ETH: 0x5e994663a04261a98a477a620295f0e9934e5c34

