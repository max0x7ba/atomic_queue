"use strict";

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

$(function() {
    const spsc_pattern = { pattern: {
        path: {
            d: 'M 0 0 L 10 10 M 9 -1 L 11 1 M -1 9 L 1 11',
            strokeWidth: 4
        },
        width: 10,
        height: 10,
        opacity: 1
    }};

    const settings = {
     "boost::lockfree::spsc_queue": [$.extend(true, {pattern: {color: '#8E44AD'}}, spsc_pattern),  0],
   "moodycamel::ReaderWriterQueue": [$.extend(true, {pattern: {color: '#E74C3C'}}, spsc_pattern),  1],
          "boost::lockfree::queue": ['#AA73C2',  2],
     "moodycamel::ConcurrentQueue": ['#ED796D',  3],
                "pthread_spinlock": ['#58D68D',  4],
                 "tbb::spin_mutex": ['#3498DB',  5],
     "tbb::speculative_spin_mutex": ['#67B2E4',  6],
   "tbb::concurrent_bounded_queue": ['#9ACCED',  7],
                     "AtomicQueue": ['#FFFF00',  8],
                    "AtomicQueueB": ['#FFFF40',  9],
                    "AtomicQueue2": ['#FFFF80', 10],
                   "AtomicQueueB2": ['#FFFFBF', 11],
             "OptimistAtomicQueue": ['#FF0000', 12],
            "OptimistAtomicQueueB": ['#FF4040', 13],
            "OptimistAtomicQueue2": ['#FF8080', 14],
           "OptimistAtomicQueueB2": ['#FFBFBF', 15],
    };

    function scalability_to_series(results) {
        return Array.from(Object.entries(results)).map(entry => {
            const name = entry[0];
            const s = settings[name];
            return {
                name: name,
                color: s[0],
                index: s[1],
                data: Array.from(Object.entries(entry[1])).map(xy => { return [parseInt(xy[0]), xy[1]]; })
            };
        });
    }

    function latency_to_series(results) {
        results = results["sec/round-trip"];
        const series = Array.from(Object.entries(results)).map(entry => {
            const name = entry[0];
            const value = entry[1];
            const s = settings[name];
            return {
                name: name,
                color: s[0],
                index: s[1],
                data: [{y: Math.round(value * 1e9), x: s[1]}]
            };
        });
        const categories = series
              .slice()
              .sort((a, b) => { return a.index - b.index; })
              .map(s => { return s.name; })
              ;
        return [series, categories];
    }

    function plot_scalability(div_id, series, title_suffix) {
        let chart = Highcharts.chart(div_id, {
            chart: { type: 'column' },
            title: { text: 'Scalability on ' + title_suffix },
            xAxis: {
                title: { text: 'number of producers, number of consumers' },
                tickInterval: 1
            },
            yAxis: {
                type: 'logarithmic',
                title: { text: 'throughput logarith, msg/sec' },
                max: 1e9
            },
            tooltip: {
                followPointer: true,
                useHTML: true,
                shared: true,
                headerFormat: '<span style="font-weight: bold; font-size: 1.2em;">{point.key} producers, {point.key} consumers</span><table>',
                pointFormat: '<tr><td style="color: {series.color}">{series.name}: </td>' +'<td style="text-align: right"><b>{point.y} msg/sec</b></td></tr>',
                footerFormat: '</table>'
            },
            plotOptions: {
                series: {
                    pointPadding: 0.2,
                    groupPadding: 0.1,
                    borderWidth: 0,
                    shadow: true
                }
            },
            series: series
        });
    }

    function plot_latency(div_id, series_categories, title_suffix) {
        const [series, categories] = series_categories;
        Highcharts.chart(div_id, {
            chart: { type: 'bar' },
            plotOptions: {
                series: {
                    pointPadding: 0.2,
                    groupPadding: 0.1,
                    borderWidth: 0,
                    shadow: true,
                    stacking: 'normal'
                },
                bar: { dataLabels: { enabled: true, align: 'left', inside: false } }
            },
            title: { text: 'Latency on ' + title_suffix },
            xAxis: { categories: categories },
            yAxis: { title: { text: 'latency, nanoseconds/round-trip' }, max: 800 },
            tooltip: { valueSuffix: ' nanoseconds' },
            series: series
        });
    }

    // TODO: load these from files.
    const scalability_7700k = {"AtomicQueue":{"1":64084749.0,"2":13360057.0,"3":13748756.0,"4":11434233.0},"AtomicQueue2":{"1":111153240.0,"2":13098997.0,"3":13564974.0,"4":11279230.0},"AtomicQueueB":{"1":107885962.0,"2":12386446.0,"3":13039011.0,"4":11295509.0},"AtomicQueueB2":{"1":55727778.0,"2":11526218.0,"3":12289916.0,"4":11430202.0},"OptimistAtomicQueue":{"1":852224132.0,"2":38247879.0,"3":45168168.0,"4":50500695.0},"OptimistAtomicQueue2":{"1":697086526.0,"2":33756673.0,"3":44836014.0,"4":48626689.0},"OptimistAtomicQueueB":{"1":76235235.0,"2":22853297.0,"3":27015879.0,"4":30049852.0},"OptimistAtomicQueueB2":{"1":49945765.0,"2":18195518.0,"3":22668553.0,"4":28657586.0},"boost::lockfree::queue":{"1":9910413.0,"2":8235183.0,"3":8144955.0,"4":6576076.0},"boost::lockfree::spsc_queue":{"1":154866468.0,"2":null,"3":null,"4":null},"moodycamel::ConcurrentQueue":{"1":24377902.0,"2":13696282.0,"3":17345890.0,"4":21083461.0},"moodycamel::ReaderWriterQueue":{"1":350338923.0,"2":null,"3":null,"4":null},"pthread_spinlock":{"1":30636583.0,"2":19747765.0,"3":15761298.0,"4":13136341.0},"tbb::concurrent_bounded_queue":{"1":15736858.0,"2":16002779.0,"3":15223886.0,"4":13759664.0},"tbb::speculative_spin_mutex":{"1":50332632.0,"2":38300791.0,"3":33147732.0,"4":24841568.0},"tbb::spin_mutex":{"1":86590685.0,"2":64774727.0,"3":47582887.0,"4":35534120.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":123231439.0,"2":4628941.0,"3":3391512.0,"4":2696917.0,"5":2234085.0,"6":2033542.0,"7":1734172.0,"8":1647240.0,"9":1471944.0,"10":1314703.0,"11":1119127.0,"12":1022561.0,"13":911501.0,"14":894957.0},"AtomicQueue2":{"1":11276604.0,"2":4824215.0,"3":3342625.0,"4":2774020.0,"5":2212914.0,"6":1906063.0,"7":1676133.0,"8":1680294.0,"9":1393167.0,"10":1307980.0,"11":1161491.0,"12":1061791.0,"13":921344.0,"14":898664.0},"AtomicQueueB":{"1":19055029.0,"2":3765812.0,"3":3137673.0,"4":2683403.0,"5":2319139.0,"6":1882475.0,"7":1804537.0,"8":1554212.0,"9":1327635.0,"10":1207512.0,"11":1134555.0,"12":984768.0,"13":928930.0,"14":861818.0},"AtomicQueueB2":{"1":19318523.0,"2":3733651.0,"3":3030753.0,"4":2664377.0,"5":2286373.0,"6":1832355.0,"7":1863559.0,"8":1502293.0,"9":1462294.0,"10":1245304.0,"11":1147857.0,"12":1015921.0,"13":898833.0,"14":877627.0},"OptimistAtomicQueue":{"1":622786115.0,"2":12658695.0,"3":13879776.0,"4":13845399.0,"5":14367426.0,"6":14391930.0,"7":14470977.0,"8":11644728.0,"9":11796028.0,"10":11286187.0,"11":11590169.0,"12":11497140.0,"13":11573726.0,"14":11473496.0},"OptimistAtomicQueue2":{"1":268248369.0,"2":11190155.0,"3":12518433.0,"4":13226909.0,"5":13725291.0,"6":13833463.0,"7":14030164.0,"8":10883745.0,"9":10683891.0,"10":10670351.0,"11":11046063.0,"12":10674717.0,"13":10802813.0,"14":10665141.0},"OptimistAtomicQueueB":{"1":88186962.0,"2":7777846.0,"3":7118911.0,"4":7152726.0,"5":7238832.0,"6":7183700.0,"7":7015588.0,"8":5930351.0,"9":5972397.0,"10":5306398.0,"11":5638484.0,"12":5344374.0,"13":4999501.0,"14":5041421.0},"OptimistAtomicQueueB2":{"1":29104730.0,"2":6284622.0,"3":5918473.0,"4":6150810.0,"5":6099041.0,"6":6067140.0,"7":6301003.0,"8":4643730.0,"9":4447088.0,"10":4419152.0,"11":3845752.0,"12":4601848.0,"13":4715034.0,"14":4901239.0},"boost::lockfree::queue":{"1":3474035.0,"2":2615935.0,"3":2356752.0,"4":2167788.0,"5":1871023.0,"6":1815522.0,"7":1700871.0,"8":1298795.0,"9":1082821.0,"10":1050685.0,"11":965179.0,"12":780840.0,"13":802696.0,"14":721414.0},"boost::lockfree::spsc_queue":{"1":171423244.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":10719122.0,"2":6106856.0,"3":6672688.0,"4":6238339.0,"5":5280237.0,"6":5412147.0,"7":4873380.0,"8":3573930.0,"9":3243740.0,"10":3234684.0,"11":3469904.0,"12":3025201.0,"13":3268309.0,"14":3054758.0},"moodycamel::ReaderWriterQueue":{"1":227830753.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":8631150.0,"2":4468148.0,"3":3462043.0,"4":2715845.0,"5":2277623.0,"6":2046286.0,"7":1783750.0,"8":1139055.0,"9":1016766.0,"10":923809.0,"11":909823.0,"12":874849.0,"13":799405.0,"14":761861.0},"tbb::concurrent_bounded_queue":{"1":6542032.0,"2":5352757.0,"3":3948476.0,"4":3406559.0,"5":2862679.0,"6":2651618.0,"7":2459383.0,"8":1920101.0,"9":1822622.0,"10":1461427.0,"11":1391941.0,"12":1237428.0,"13":1133267.0,"14":1019221.0},"tbb::speculative_spin_mutex":{"1":22635581.0,"2":10325534.0,"3":7015447.0,"4":4967653.0,"5":4699717.0,"6":3664207.0,"7":3181753.0,"8":2595255.0,"9":2011693.0,"10":1726537.0,"11":1378347.0,"12":1264650.0,"13":1141997.0,"14":1196402.0},"tbb::spin_mutex":{"1":25482138.0,"2":12387520.0,"3":10626484.0,"4":7055121.0,"5":5153780.0,"6":3934024.0,"7":2730806.0,"8":1579142.0,"9":884982.0,"10":721151.0,"11":594113.0,"12":567297.0,"13":474981.0,"14":399293.0}};
    const latency_7700k = {"sec/round-trip":{"AtomicQueue":0.000000134,"AtomicQueue2":0.000000135,"AtomicQueueB":0.000000139,"AtomicQueueB2":0.000000169,"OptimistAtomicQueue":0.000000119,"OptimistAtomicQueue2":0.000000149,"OptimistAtomicQueueB":0.000000137,"OptimistAtomicQueueB2":0.000000165,"boost::lockfree::queue":0.000000265,"boost::lockfree::spsc_queue":0.000000117,"moodycamel::ConcurrentQueue":0.000000212,"moodycamel::ReaderWriterQueue":0.000000109,"pthread_spinlock":0.000000283,"tbb::concurrent_bounded_queue":0.00000025,"tbb::speculative_spin_mutex":0.000000645,"tbb::spin_mutex":0.00000024}};
    const latency_xeon_gold_6132 = {"sec/round-trip":{"AtomicQueue":0.000000236,"AtomicQueue2":0.000000312,"AtomicQueueB":0.000000331,"AtomicQueueB2":0.000000397,"OptimistAtomicQueue":0.000000285,"OptimistAtomicQueue2":0.00000033,"OptimistAtomicQueueB":0.000000362,"OptimistAtomicQueueB2":0.000000415,"boost::lockfree::queue":0.000000755,"boost::lockfree::spsc_queue":0.000000272,"moodycamel::ConcurrentQueue":0.000000449,"moodycamel::ReaderWriterQueue":0.000000239,"pthread_spinlock":0.000000643,"tbb::concurrent_bounded_queue":0.000000601,"tbb::speculative_spin_mutex":0.000000636,"tbb::spin_mutex":0.000000572}};

    plot_scalability('scalability-7700k-5GHz', scalability_to_series(scalability_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-7700k-5GHz', latency_to_series(latency_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
