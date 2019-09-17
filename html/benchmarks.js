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
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":11050069.0,"2":6198745.0,"3":4277145.0,"4":3396968.0,"5":2661521.0,"6":2356696.0,"7":2089956.0,"8":2083067.0,"9":1905669.0,"10":1775847.0,"11":1623047.0,"12":1490402.0,"13":1382775.0,"14":1221800.0},"AtomicQueue2":{"1":10337098.0,"2":5419859.0,"3":3785265.0,"4":3081503.0,"5":2672078.0,"6":2394399.0,"7":2092404.0,"8":2064861.0,"9":1927853.0,"10":1769291.0,"11":1627808.0,"12":1473565.0,"13":1350332.0,"14":1231725.0},"AtomicQueueB":{"1":6815959.0,"2":4208122.0,"3":3617955.0,"4":2997962.0,"5":2578571.0,"6":2257473.0,"7":2016212.0,"8":1896391.0,"9":1795078.0,"10":1658972.0,"11":1530549.0,"12":1406050.0,"13":1295884.0,"14":1160739.0},"AtomicQueueB2":{"1":6210927.0,"2":4253058.0,"3":3318842.0,"4":2973279.0,"5":2597813.0,"6":2290284.0,"7":2106135.0,"8":1884390.0,"9":1814628.0,"10":1661780.0,"11":1522012.0,"12":1408165.0,"13":1284669.0,"14":1169493.0},"OptimistAtomicQueue":{"1":26651459.0,"2":13495689.0,"3":14865240.0,"4":15058209.0,"5":14972467.0,"6":15057441.0,"7":14937907.0,"8":12507474.0,"9":12705761.0,"10":12473830.0,"11":11309788.0,"12":12368412.0,"13":11823226.0,"14":11890482.0},"OptimistAtomicQueue2":{"1":14396323.0,"2":12283052.0,"3":13213355.0,"4":13880844.0,"5":14223223.0,"6":14269415.0,"7":14368916.0,"8":11517246.0,"9":11752308.0,"10":11827885.0,"11":12045111.0,"12":11702915.0,"13":12070419.0,"14":11515263.0},"OptimistAtomicQueueB":{"1":10782030.0,"2":8305476.0,"3":8120663.0,"4":8212534.0,"5":8170771.0,"6":8100366.0,"7":8008213.0,"8":6912548.0,"9":6793981.0,"10":6556746.0,"11":6475616.0,"12":6333931.0,"13":6318638.0,"14":5924674.0},"OptimistAtomicQueueB2":{"1":5972846.0,"2":4821043.0,"3":4795790.0,"4":4861002.0,"5":4968962.0,"6":4980163.0,"7":5004683.0,"8":4316131.0,"9":4170509.0,"10":4066869.0,"11":3999486.0,"12":3774101.0,"13":3795603.0,"14":3725963.0},"boost::lockfree::queue":{"1":3448711.0,"2":2794738.0,"3":2564086.0,"4":2435305.0,"5":2376293.0,"6":2241423.0,"7":2106128.0,"8":1649967.0,"9":1503182.0,"10":1345819.0,"11":1178885.0,"12":1121563.0,"13":1065915.0,"14":1040952.0},"boost::lockfree::spsc_queue":{"1":30041701.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":9696923.0,"2":6313069.0,"3":6538816.0,"4":7043995.0,"5":7032619.0,"6":7180125.0,"7":7390785.0,"8":5973520.0,"9":5446824.0,"10":5093895.0,"11":4770697.0,"12":4999723.0,"13":4989150.0,"14":4998936.0},"moodycamel::ReaderWriterQueue":{"1":54387013.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":11212816.0,"2":6577764.0,"3":4845149.0,"4":3697822.0,"5":3452104.0,"6":2894136.0,"7":2448814.0,"8":2001470.0,"9":1808630.0,"10":1577464.0,"11":1416877.0,"12":1308833.0,"13":1273303.0,"14":1179877.0},"tbb::concurrent_bounded_queue":{"1":7211785.0,"2":5942125.0,"3":4380113.0,"4":3759590.0,"5":3344296.0,"6":2957090.0,"7":2684516.0,"8":2310906.0,"9":2207616.0,"10":2029127.0,"11":1816561.0,"12":1643677.0,"13":1495406.0,"14":1427450.0},"tbb::speculative_spin_mutex":{"1":28412287.0,"2":14819402.0,"3":9152551.0,"4":6314426.0,"5":5138448.0,"6":4150614.0,"7":3418740.0,"8":3091424.0,"9":2514984.0,"10":2022246.0,"11":1730672.0,"12":1497546.0,"13":1363363.0,"14":1258148.0},"tbb::spin_mutex":{"1":30728871.0,"2":19031371.0,"3":11489516.0,"4":5074346.0,"5":2895442.0,"6":2107725.0,"7":1715054.0,"8":933990.0,"9":899228.0,"10":943804.0,"11":867138.0,"12":765946.0,"13":678502.0,"14":607776.0}};
    const latency_7700k = {"sec/round-trip":{"AtomicQueue":0.000000152,"AtomicQueue2":0.000000182,"AtomicQueueB":0.000000168,"AtomicQueueB2":0.000000226,"OptimistAtomicQueue":0.000000096,"OptimistAtomicQueue2":0.000000167,"OptimistAtomicQueueB":0.000000126,"OptimistAtomicQueueB2":0.000000199,"boost::lockfree::queue":0.000000265,"boost::lockfree::spsc_queue":0.000000122,"moodycamel::ConcurrentQueue":0.000000215,"moodycamel::ReaderWriterQueue":0.000000124,"pthread_spinlock":0.000000574,"tbb::concurrent_bounded_queue":0.000000254,"tbb::speculative_spin_mutex":0.00000049,"tbb::spin_mutex":0.000000198}};
    const latency_xeon_gold_6132 = {"sec/round-trip":{"AtomicQueue":0.000000361,"AtomicQueue2":0.000000369,"AtomicQueueB":0.000000392,"AtomicQueueB2":0.000000452,"OptimistAtomicQueue":0.000000221,"OptimistAtomicQueue2":0.000000241,"OptimistAtomicQueueB":0.000000313,"OptimistAtomicQueueB2":0.000000447,"boost::lockfree::queue":0.000000736,"boost::lockfree::spsc_queue":0.000000268,"moodycamel::ConcurrentQueue":0.000000442,"moodycamel::ReaderWriterQueue":0.00000027,"pthread_spinlock":0.000000369,"tbb::concurrent_bounded_queue":0.000000617,"tbb::speculative_spin_mutex":0.000000843,"tbb::spin_mutex":0.000000271}};

    plot_scalability('scalability-7700k-5GHz', scalability_to_series(scalability_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-7700k-5GHz', latency_to_series(latency_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
