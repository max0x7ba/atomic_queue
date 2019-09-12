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
        Highcharts.chart(div_id, {
            chart: { type: 'column' },
            title: { text: 'Scalability on ' + title_suffix },
            xAxis: {
                title: { text: 'number of producers, number of consumers' },
                tickInterval: 1
            },
            yAxis: { title: { text: 'throughput, msg/sec' } },
            tooltip: {
                followPointer: true,
                useHTML: true,
                shared: true,
                headerFormat: '<span style="font-weight: bold; font-size: 1.2em;">{point.key} producers, {point.key} consumers</span><table>',
                pointFormat: '<tr><td style="color: {series.color}">{series.name}: </td>' +'<td style="text-align: right"><b>{point.y} msg/sec</b></td></tr>',
                footerFormat: '</table>'
            },
            series: series
        });
    }

    function plot_latency(div_id, series_categories, title_suffix) {
        const [series, categories] = series_categories;
        Highcharts.chart(div_id, {
            chart: { type: 'bar' },
            plotOptions: {
                series: { stacking: 'normal' },
                bar: { dataLabels: { enabled: true, align: 'left', inside: false } }
            },
            title: { text: 'Latency on ' + title_suffix },
            xAxis: { categories: categories },
            yAxis: { title: { text: 'latency, nanoseconds/round-trip' }, max: 1000 },
            tooltip: { valueSuffix: ' nanoseconds' },
            series: series
        });
    }

    // TODO: load these from files.
    const scalability_7700k = {"AtomicQueue":{"1":22512209.0,"2":13481026.0,"3":12640779.0,"4":12950135.0},"AtomicQueue2":{"1":21748526.0,"2":13035596.0,"3":12403749.0,"4":12621381.0},"AtomicQueueB":{"1":18010750.0,"2":12055548.0,"3":12022428.0,"4":11648587.0},"AtomicQueueB2":{"1":12948011.0,"2":11096881.0,"3":10983029.0,"4":11297398.0},"OptimistAtomicQueue":{"1":113403212.0,"2":76759529.0,"3":45613946.0,"4":50350863.0},"OptimistAtomicQueue2":{"1":97419324.0,"2":31778412.0,"3":45310404.0,"4":48456971.0},"OptimistAtomicQueueB":{"1":21022929.0,"2":22132083.0,"3":27067298.0,"4":30313668.0},"OptimistAtomicQueueB2":{"1":12512443.0,"2":13335097.0,"3":18911078.0,"4":22698580.0},"boost::lockfree::queue":{"1":9514709.0,"2":8251100.0,"3":7804134.0,"4":6473675.0},"boost::lockfree::spsc_queue":{"1":89470944.0,"2":null,"3":null,"4":null},"moodycamel::ConcurrentQueue":{"1":24406016.0,"2":13248971.0,"3":18317634.0,"4":21204262.0},"moodycamel::ReaderWriterQueue":{"1":108942300.0,"2":null,"3":null,"4":null},"pthread_spinlock":{"1":23959320.0,"2":18426782.0,"3":15329084.0,"4":14388629.0},"tbb::concurrent_bounded_queue":{"1":15689026.0,"2":16221226.0,"3":15285375.0,"4":13686422.0},"tbb::speculative_spin_mutex":{"1":45110458.0,"2":37863157.0,"3":32800311.0,"4":24665346.0},"tbb::spin_mutex":{"1":58490075.0,"2":40144884.0,"3":42113870.0,"4":35844503.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":2853223.0,"2":3609379.0,"3":2241787.0,"4":1978378.0,"5":1769950.0,"6":1590624.0,"7":1412812.0,"8":2047431.0,"9":1936091.0,"10":1701065.0,"11":1573690.0,"12":1759322.0,"13":1647314.0,"14":1352238.0},"AtomicQueue2":{"1":2577730.0,"2":2981780.0,"3":2248177.0,"4":1947457.0,"5":1756653.0,"6":1592400.0,"7":1393690.0,"8":2147964.0,"9":2047732.0,"10":1904622.0,"11":1654375.0,"12":1616432.0,"13":1355658.0,"14":1483223.0},"AtomicQueueB":{"1":2850258.0,"2":2439685.0,"3":2002094.0,"4":1827042.0,"5":1670932.0,"6":1532278.0,"7":1410099.0,"8":1871942.0,"9":1845213.0,"10":1654081.0,"11":1506872.0,"12":1522883.0,"13":1249799.0,"14":1308632.0},"AtomicQueueB2":{"1":2719510.0,"2":2392318.0,"3":1918525.0,"4":1804191.0,"5":1639709.0,"6":1527937.0,"7":1509028.0,"8":1905319.0,"9":1819485.0,"10":1617635.0,"11":1478313.0,"12":1612544.0,"13":1533993.0,"14":1318198.0},"OptimistAtomicQueue":{"1":32827585.0,"2":8403929.0,"3":9214007.0,"4":10924055.0,"5":11170844.0,"6":11649928.0,"7":11983121.0,"8":12255833.0,"9":12170575.0,"10":12218988.0,"11":12249822.0,"12":12180715.0,"13":12330398.0,"14":11896122.0},"OptimistAtomicQueue2":{"1":4225612.0,"2":6411813.0,"3":8755366.0,"4":10438530.0,"5":10896608.0,"6":11360469.0,"7":11717151.0,"8":11223638.0,"9":11546961.0,"10":11633166.0,"11":11717810.0,"12":11685614.0,"13":11452486.0,"14":11771256.0},"OptimistAtomicQueueB":{"1":15525179.0,"2":5429332.0,"3":5988733.0,"4":6323618.0,"5":6299309.0,"6":6571465.0,"7":6076228.0,"8":6717652.0,"9":6461929.0,"10":6329894.0,"11":6246921.0,"12":6312812.0,"13":5888228.0,"14":5983549.0},"OptimistAtomicQueueB2":{"1":8299699.0,"2":3507325.0,"3":3387536.0,"4":3529829.0,"5":3549787.0,"6":3606720.0,"7":3649593.0,"8":4185218.0,"9":3971042.0,"10":3908388.0,"11":3859188.0,"12":3696534.0,"13":3746038.0,"14":3595451.0},"boost::lockfree::queue":{"1":1971345.0,"2":1640568.0,"3":1618417.0,"4":1589663.0,"5":1547246.0,"6":1481523.0,"7":1459624.0,"8":1575359.0,"9":1410539.0,"10":1284552.0,"11":1201563.0,"12":1078418.0,"13":1049579.0,"14":1116217.0},"boost::lockfree::spsc_queue":{"1":56614747.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":5884285.0,"2":2653641.0,"3":3349634.0,"4":4156271.0,"5":4793797.0,"6":5285532.0,"7":5836175.0,"8":5945630.0,"9":5503548.0,"10":5197864.0,"11":4827392.0,"12":4911050.0,"13":4680596.0,"14":4782748.0},"moodycamel::ReaderWriterQueue":{"1":34314888.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":3112809.0,"2":2926598.0,"3":2365220.0,"4":2072157.0,"5":1851504.0,"6":1701352.0,"7":1508156.0,"8":1968552.0,"9":1752979.0,"10":1552359.0,"11":1418790.0,"12":1325084.0,"13":1239936.0,"14":1186670.0},"tbb::concurrent_bounded_queue":{"1":2304309.0,"2":3284497.0,"3":3302768.0,"4":2639715.0,"5":2164859.0,"6":1940959.0,"7":1780940.0,"8":2302857.0,"9":2133510.0,"10":1924869.0,"11":1741828.0,"12":1457302.0,"13":1513302.0,"14":1461897.0},"tbb::speculative_spin_mutex":{"1":19695211.0,"2":7033114.0,"3":4238738.0,"4":3154655.0,"5":2540875.0,"6":2314788.0,"7":2132709.0,"8":2853531.0,"9":2553327.0,"10":2129079.0,"11":1724924.0,"12":1620464.0,"13":1793766.0,"14":1625391.0},"tbb::spin_mutex":{"1":22128225.0,"2":12177212.0,"3":7419173.0,"4":3996112.0,"5":2500020.0,"6":1720005.0,"7":1214022.0,"8":1105889.0,"9":1064685.0,"10":1108508.0,"11":1044664.0,"12":1062520.0,"13":1091252.0,"14":908662.0}};
    const latency_7700k = {"sec/round-trip":{"AtomicQueue":0.000000152,"AtomicQueue2":0.000000182,"AtomicQueueB":0.000000168,"AtomicQueueB2":0.000000226,"OptimistAtomicQueue":0.000000096,"OptimistAtomicQueue2":0.000000167,"OptimistAtomicQueueB":0.000000126,"OptimistAtomicQueueB2":0.000000199,"boost::lockfree::queue":0.000000265,"boost::lockfree::spsc_queue":0.000000122,"moodycamel::ConcurrentQueue":0.000000215,"moodycamel::ReaderWriterQueue":0.000000124,"pthread_spinlock":0.000000574,"tbb::concurrent_bounded_queue":0.000000254,"tbb::speculative_spin_mutex":0.00000049,"tbb::spin_mutex":0.000000198}};
    const latency_xeon_gold_6132 = {"sec/round-trip":{"AtomicQueue":0.000000391,"AtomicQueue2":0.000000407,"AtomicQueueB":0.000000418,"AtomicQueueB2":0.000000472,"OptimistAtomicQueue":0.000000237,"OptimistAtomicQueue2":0.000000252,"OptimistAtomicQueueB":0.000000324,"OptimistAtomicQueueB2":0.000000449,"boost::lockfree::queue":0.000000739,"boost::lockfree::spsc_queue":0.000000294,"moodycamel::ConcurrentQueue":0.000000429,"moodycamel::ReaderWriterQueue":0.000000321,"pthread_spinlock":0.000000392,"tbb::concurrent_bounded_queue":0.000000625,"tbb::speculative_spin_mutex":0.000000934,"tbb::spin_mutex":0.000000353}};

    plot_scalability('scalability-7700k-5GHz', scalability_to_series(scalability_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-7700k-5GHz', latency_to_series(latency_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
