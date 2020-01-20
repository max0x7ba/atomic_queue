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
   "tbb::concurrent_bounded_queue": ['#9ACCED',  6],
                     "AtomicQueue": ['#FFFF00',  7],
                    "AtomicQueueB": ['#FFFF40',  8],
                    "AtomicQueue2": ['#FFFF80',  9],
                   "AtomicQueueB2": ['#FFFFBF', 10],
             "OptimistAtomicQueue": ['#FF0000', 11],
            "OptimistAtomicQueueB": ['#FF4040', 12],
            "OptimistAtomicQueue2": ['#FF8080', 13],
           "OptimistAtomicQueueB2": ['#FFBFBF', 14],
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
            yAxis: { title: { text: 'latency, nanoseconds/round-trip' }, max: 700 },
            tooltip: { valueSuffix: ' nanoseconds' },
            series: series
        });
    }

    // TODO: load these from files.
    const scalability_9900KS = {"AtomicQueue":{"1":301561447.0,"2":12520152.0,"3":10914287.0,"4":8268873.0,"5":8181077.0,"6":8001280.0,"7":8030067.0,"8":7519277.0},"AtomicQueue2":{"1":24652882.0,"2":12127949.0,"3":10186299.0,"4":8133831.0,"5":8111845.0,"6":7942380.0,"7":7965498.0,"8":7427562.0},"AtomicQueueB":{"1":230791271.0,"2":12100863.0,"3":11116064.0,"4":8221578.0,"5":7939528.0,"6":7621657.0,"7":7785393.0,"8":7225373.0},"AtomicQueueB2":{"1":56143379.0,"2":11262466.0,"3":9577584.0,"4":7849112.0,"5":7735283.0,"6":7447186.0,"7":7679252.0,"8":7120325.0},"OptimistAtomicQueue":{"1":827243100.0,"2":32793965.0,"3":37561910.0,"4":39464775.0,"5":48711199.0,"6":50577426.0,"7":53205853.0,"8":50617291.0},"OptimistAtomicQueue2":{"1":683436723.0,"2":29709035.0,"3":33415151.0,"4":35574988.0,"5":43162677.0,"6":49628942.0,"7":51881631.0,"8":49984492.0},"OptimistAtomicQueueB":{"1":804033352.0,"2":32454906.0,"3":37045586.0,"4":38788935.0,"5":47479405.0,"6":49224042.0,"7":51769480.0,"8":49263631.0},"OptimistAtomicQueueB2":{"1":159673423.0,"2":27941976.0,"3":31948919.0,"4":31869639.0,"5":38049134.0,"6":41435707.0,"7":44304810.0,"8":46014506.0},"boost::lockfree::queue":{"1":8202017.0,"2":7794496.0,"3":7713569.0,"4":7176199.0,"5":6862329.0,"6":6376647.0,"7":6254734.0,"8":5847247.0},"boost::lockfree::spsc_queue":{"1":78806879.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null},"moodycamel::ConcurrentQueue":{"1":23065541.0,"2":14332033.0,"3":13654159.0,"4":14921051.0,"5":18493994.0,"6":19460013.0,"7":20113249.0,"8":20621189.0},"moodycamel::ReaderWriterQueue":{"1":289157615.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null},"pthread_spinlock":{"1":27274590.0,"2":16321051.0,"3":12185530.0,"4":9206787.0,"5":10604837.0,"6":9328111.0,"7":8589116.0,"8":7634929.0},"tbb::concurrent_bounded_queue":{"1":14858192.0,"2":14960108.0,"3":12561546.0,"4":10641693.0,"5":10209087.0,"6":9543777.0,"7":8703630.0,"8":8027878.0},"tbb::spin_mutex":{"1":38664637.0,"2":21590404.0,"3":11700568.0,"4":6505706.0,"5":5995991.0,"6":5389823.0,"7":4885591.0,"8":4271422.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":158109112.0,"2":4921854.0,"3":3498735.0,"4":2896774.0,"5":2416926.0,"6":2046932.0,"7":1773634.0,"8":1645924.0,"9":1457036.0,"10":1322161.0,"11":1186336.0,"12":1072455.0,"13":930567.0,"14":931606.0},"AtomicQueue2":{"1":130966968.0,"2":4620760.0,"3":3305710.0,"4":2787070.0,"5":2364350.0,"6":1972774.0,"7":1816863.0,"8":1715741.0,"9":1543989.0,"10":1362488.0,"11":1200436.0,"12":1066522.0,"13":956885.0,"14":883559.0},"AtomicQueueB":{"1":150200425.0,"2":4731025.0,"3":3368096.0,"4":2829384.0,"5":2408528.0,"6":1979764.0,"7":1855659.0,"8":1707383.0,"9":1467147.0,"10":1362266.0,"11":1257940.0,"12":1118451.0,"13":986849.0,"14":911597.0},"AtomicQueueB2":{"1":30885730.0,"2":4940112.0,"3":3295637.0,"4":2695437.0,"5":2257248.0,"6":2044260.0,"7":1831373.0,"8":1714119.0,"9":1446334.0,"10":1345247.0,"11":1146609.0,"12":1102961.0,"13":951675.0,"14":946796.0},"OptimistAtomicQueue":{"1":615462112.0,"2":12588449.0,"3":13517952.0,"4":14099926.0,"5":14555742.0,"6":14477634.0,"7":14589043.0,"8":11942734.0,"9":12318122.0,"10":11652615.0,"11":11276576.0,"12":11790362.0,"13":11616924.0,"14":11580480.0},"OptimistAtomicQueue2":{"1":285701790.0,"2":11464345.0,"3":12643790.0,"4":13373738.0,"5":13587917.0,"6":13787959.0,"7":14214689.0,"8":11068029.0,"9":11508394.0,"10":10943725.0,"11":10735351.0,"12":10831674.0,"13":10856099.0,"14":11070676.0},"OptimistAtomicQueueB":{"1":392396088.0,"2":12772847.0,"3":13333742.0,"4":13799277.0,"5":14338043.0,"6":14249719.0,"7":14319209.0,"8":12205595.0,"9":11696373.0,"10":11075294.0,"11":11768276.0,"12":11481230.0,"13":11334782.0,"14":11157997.0},"OptimistAtomicQueueB2":{"1":52277970.0,"2":11010593.0,"3":11902777.0,"4":12363497.0,"5":12904686.0,"6":13074313.0,"7":13206227.0,"8":10537499.0,"9":10484867.0,"10":10087570.0,"11":10107976.0,"12":9929433.0,"13":10750117.0,"14":10061327.0},"boost::lockfree::queue":{"1":3509287.0,"2":2691360.0,"3":2524041.0,"4":2279338.0,"5":2090858.0,"6":1923587.0,"7":1794532.0,"8":1295226.0,"9":1214404.0,"10":1030892.0,"11":948879.0,"12":894742.0,"13":768881.0,"14":782735.0},"boost::lockfree::spsc_queue":{"1":192419130.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":11324231.0,"2":6256475.0,"3":6277392.0,"4":6300071.0,"5":5622547.0,"6":5854465.0,"7":5134036.0,"8":3802947.0,"9":3549189.0,"10":3286559.0,"11":3416412.0,"12":3376207.0,"13":3319388.0,"14":3502120.0},"moodycamel::ReaderWriterQueue":{"1":275435749.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":9636407.0,"2":4638371.0,"3":3549542.0,"4":2780490.0,"5":2484911.0,"6":2042073.0,"7":1893618.0,"8":1317140.0,"9":1074015.0,"10":934007.0,"11":912801.0,"12":852631.0,"13":827944.0,"14":823481.0},"tbb::concurrent_bounded_queue":{"1":6767479.0,"2":5453622.0,"3":4145085.0,"4":3564610.0,"5":3010331.0,"6":2587858.0,"7":2440643.0,"8":2068666.0,"9":2058159.0,"10":1739814.0,"11":1378381.0,"12":1234436.0,"13":1122814.0,"14":1015363.0},"tbb::spin_mutex":{"1":20199929.0,"2":11734715.0,"3":7460630.0,"4":5116921.0,"5":4793972.0,"6":3313624.0,"7":2245725.0,"8":1473631.0,"9":943642.0,"10":757081.0,"11":575810.0,"12":492764.0,"13":486487.0,"14":424400.0}};
    const latency_9900KS = {"AtomicQueue":0.000000159,"AtomicQueue2":0.000000172,"AtomicQueueB":0.000000167,"AtomicQueueB2":0.000000177,"OptimistAtomicQueue":0.000000144,"OptimistAtomicQueue2":0.000000167,"OptimistAtomicQueueB":0.00000014,"OptimistAtomicQueueB2":0.000000147,"boost::lockfree::queue":0.000000303,"boost::lockfree::spsc_queue":0.000000127,"moodycamel::ConcurrentQueue":0.000000216,"moodycamel::ReaderWriterQueue":0.000000107,"pthread_spinlock":0.000000242,"tbb::concurrent_bounded_queue":0.000000269,"tbb::spin_mutex":0.00000024};
    const latency_xeon_gold_6132 = {"AtomicQueue":0.000000233,"AtomicQueue2":0.000000309,"AtomicQueueB":0.000000333,"AtomicQueueB2":0.000000387,"OptimistAtomicQueue":0.000000284,"OptimistAtomicQueue2":0.000000326,"OptimistAtomicQueueB":0.000000324,"OptimistAtomicQueueB2":0.00000035,"boost::lockfree::queue":0.000000695,"boost::lockfree::spsc_queue":0.000000256,"moodycamel::ConcurrentQueue":0.000000393,"moodycamel::ReaderWriterQueue":0.00000022,"pthread_spinlock":0.000000649,"tbb::concurrent_bounded_queue":0.000000593,"tbb::spin_mutex":0.000000515};

    plot_scalability('scalability-9900KS-5GHz', scalability_to_series(scalability_9900KS), "Intel i9-9900KS (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-9900KS-5GHz', latency_to_series(latency_9900KS), "Intel i9-9900KS (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
