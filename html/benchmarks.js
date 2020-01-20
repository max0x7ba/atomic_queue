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
                "pthread_spinlock": ['#58D68D',  2],
                      "std::mutex": ['#239B56',  3],
                 "tbb::spin_mutex": ['#3498DB',  4],
   "tbb::concurrent_bounded_queue": ['#9ACCED',  5],
          "boost::lockfree::queue": ['#AA73C2',  6],
     "moodycamel::ConcurrentQueue": ['#ED796D',  7],
     "xenium::michael_scott_queue": ['#73C6B6',  8],
         "xenium::ramalhete_queue": ['#45B39D',  9],
    "xenium::vyukov_bounded_queue": ['#16A085', 10],
                     "AtomicQueue": ['#FFFF00', 11],
                    "AtomicQueueB": ['#FFFF40', 12],
                    "AtomicQueue2": ['#FFFF80', 13],
                   "AtomicQueueB2": ['#FFFFBF', 14],
             "OptimistAtomicQueue": ['#FF0000', 15],
            "OptimistAtomicQueueB": ['#FF4040', 16],
            "OptimistAtomicQueue2": ['#FF8080', 17],
           "OptimistAtomicQueueB2": ['#FFBFBF', 18]
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

    function plot_scalability(div_id, series, title_suffix, max_lin, max_log) {
        const modes = [
            {type: 'linear', title: { text: 'throughput, msg/sec (linear scale)'}, max: max_lin},
            {type: 'logarithmic', title: { text: 'throughput, msg/sec (logarithmic scale)'}, max: max_log},
        ];
        let mode = 0;
        let chart = Highcharts.chart(div_id, {
            chart: {
                type: 'column',
                events: {
                    click: function() {
                        mode ^= 1;
                        chart.yAxis[0].update(modes[mode]);
                    }
                }
            },
            title: { text: 'Scalability on ' + title_suffix },
            subtitle: { text: "click on the chart background to switch between linear and logarithmic scales" },
            xAxis: {
                title: { text: 'number of producers, number of consumers' },
                tickInterval: 1
            },
            yAxis: modes[mode],
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
    const scalability_9900KS = {"AtomicQueue":{"1":23640369.0,"2":12943441.0,"3":10256253.0,"4":8304660.0,"5":8244465.0,"6":8029302.0,"7":8058835.0,"8":7442234.0},"AtomicQueue2":{"1":10746078.0,"2":11146434.0,"3":9414380.0,"4":7904506.0,"5":7855126.0,"6":7658244.0,"7":7889511.0,"8":7470767.0},"AtomicQueueB":{"1":18660569.0,"2":12292384.0,"3":10011099.0,"4":8094664.0,"5":7793340.0,"6":7609454.0,"7":7726185.0,"8":7142938.0},"AtomicQueueB2":{"1":14738377.0,"2":10956892.0,"3":8916403.0,"4":7717734.0,"5":7572563.0,"6":7577323.0,"7":7801267.0,"8":7198930.0},"OptimistAtomicQueue":{"1":65326109.0,"2":33963374.0,"3":37490846.0,"4":39346437.0,"5":46466767.0,"6":51914982.0,"7":54819586.0,"8":51472934.0},"OptimistAtomicQueue2":{"1":18100895.0,"2":29159981.0,"3":33320492.0,"4":35077707.0,"5":35251901.0,"6":36625135.0,"7":38377375.0,"8":39902762.0},"OptimistAtomicQueueB":{"1":56334546.0,"2":34077933.0,"3":37008195.0,"4":38798564.0,"5":45319486.0,"6":51400150.0,"7":53866248.0,"8":50560456.0},"OptimistAtomicQueueB2":{"1":22278834.0,"2":25792872.0,"3":28804275.0,"4":31799124.0,"5":35128645.0,"6":37928442.0,"7":39199589.0,"8":40793698.0},"boost::lockfree::queue":{"1":8260647.0,"2":7800968.0,"3":7603347.0,"4":7097095.0,"5":6872844.0,"6":6387388.0,"7":6170695.0,"8":5834230.0},"boost::lockfree::spsc_queue":{"1":79092145.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null},"moodycamel::ConcurrentQueue":{"1":23189568.0,"2":15080986.0,"3":13951343.0,"4":14923133.0,"5":19016106.0,"6":19893649.0,"7":20708687.0,"8":20938407.0},"moodycamel::ReaderWriterQueue":{"1":295716975.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null},"pthread_spinlock":{"1":27229004.0,"2":16192208.0,"3":12181215.0,"4":9094456.0,"5":10571006.0,"6":9315636.0,"7":8583159.0,"8":7597199.0},"std::mutex":{"1":8037432.0,"2":6475278.0,"3":6503282.0,"4":6821664.0,"5":6622013.0,"6":6461594.0,"7":6278018.0,"8":6068950.0},"tbb::concurrent_bounded_queue":{"1":14800295.0,"2":15217234.0,"3":12782825.0,"4":10685498.0,"5":10198063.0,"6":9517751.0,"7":8691071.0,"8":8002795.0},"tbb::spin_mutex":{"1":41437276.0,"2":21828238.0,"3":11612637.0,"4":6693671.0,"5":6049098.0,"6":5428307.0,"7":4878552.0,"8":4244203.0},"xenium::michael_scott_queue":{"1":10215151.0,"2":8700295.0,"3":7804470.0,"4":6570763.0,"5":6836260.0,"6":7109682.0,"7":6705169.0,"8":6300559.0},"xenium::ramalhete_queue":{"1":33427748.0,"2":24557805.0,"3":29519283.0,"4":34093641.0,"5":38783113.0,"6":40289756.0,"7":42287643.0,"8":44722686.0},"xenium::vyukov_bounded_queue":{"1":122621777.0,"2":29341966.0,"3":16399996.0,"4":12759154.0,"5":11548899.0,"6":12816029.0,"7":10425842.0,"8":8568559.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":158109112.0,"2":4921854.0,"3":3498735.0,"4":2896774.0,"5":2416926.0,"6":2046932.0,"7":1773634.0,"8":1645924.0,"9":1457036.0,"10":1322161.0,"11":1186336.0,"12":1072455.0,"13":930567.0,"14":931606.0},"AtomicQueue2":{"1":130966968.0,"2":4620760.0,"3":3305710.0,"4":2787070.0,"5":2364350.0,"6":1972774.0,"7":1816863.0,"8":1715741.0,"9":1543989.0,"10":1362488.0,"11":1200436.0,"12":1066522.0,"13":956885.0,"14":883559.0},"AtomicQueueB":{"1":150200425.0,"2":4731025.0,"3":3368096.0,"4":2829384.0,"5":2408528.0,"6":1979764.0,"7":1855659.0,"8":1707383.0,"9":1467147.0,"10":1362266.0,"11":1257940.0,"12":1118451.0,"13":986849.0,"14":911597.0},"AtomicQueueB2":{"1":30885730.0,"2":4940112.0,"3":3295637.0,"4":2695437.0,"5":2257248.0,"6":2044260.0,"7":1831373.0,"8":1714119.0,"9":1446334.0,"10":1345247.0,"11":1146609.0,"12":1102961.0,"13":951675.0,"14":946796.0},"OptimistAtomicQueue":{"1":615462112.0,"2":12588449.0,"3":13517952.0,"4":14099926.0,"5":14555742.0,"6":14477634.0,"7":14589043.0,"8":11942734.0,"9":12318122.0,"10":11652615.0,"11":11276576.0,"12":11790362.0,"13":11616924.0,"14":11580480.0},"OptimistAtomicQueue2":{"1":285701790.0,"2":11464345.0,"3":12643790.0,"4":13373738.0,"5":13587917.0,"6":13787959.0,"7":14214689.0,"8":11068029.0,"9":11508394.0,"10":10943725.0,"11":10735351.0,"12":10831674.0,"13":10856099.0,"14":11070676.0},"OptimistAtomicQueueB":{"1":392396088.0,"2":12772847.0,"3":13333742.0,"4":13799277.0,"5":14338043.0,"6":14249719.0,"7":14319209.0,"8":12205595.0,"9":11696373.0,"10":11075294.0,"11":11768276.0,"12":11481230.0,"13":11334782.0,"14":11157997.0},"OptimistAtomicQueueB2":{"1":52277970.0,"2":11010593.0,"3":11902777.0,"4":12363497.0,"5":12904686.0,"6":13074313.0,"7":13206227.0,"8":10537499.0,"9":10484867.0,"10":10087570.0,"11":10107976.0,"12":9929433.0,"13":10750117.0,"14":10061327.0},"boost::lockfree::queue":{"1":3509287.0,"2":2691360.0,"3":2524041.0,"4":2279338.0,"5":2090858.0,"6":1923587.0,"7":1794532.0,"8":1295226.0,"9":1214404.0,"10":1030892.0,"11":948879.0,"12":894742.0,"13":768881.0,"14":782735.0},"boost::lockfree::spsc_queue":{"1":192419130.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":11324231.0,"2":6256475.0,"3":6277392.0,"4":6300071.0,"5":5622547.0,"6":5854465.0,"7":5134036.0,"8":3802947.0,"9":3549189.0,"10":3286559.0,"11":3416412.0,"12":3376207.0,"13":3319388.0,"14":3502120.0},"moodycamel::ReaderWriterQueue":{"1":275435749.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":9636407.0,"2":4638371.0,"3":3549542.0,"4":2780490.0,"5":2484911.0,"6":2042073.0,"7":1893618.0,"8":1317140.0,"9":1074015.0,"10":934007.0,"11":912801.0,"12":852631.0,"13":827944.0,"14":823481.0},"tbb::concurrent_bounded_queue":{"1":6767479.0,"2":5453622.0,"3":4145085.0,"4":3564610.0,"5":3010331.0,"6":2587858.0,"7":2440643.0,"8":2068666.0,"9":2058159.0,"10":1739814.0,"11":1378381.0,"12":1234436.0,"13":1122814.0,"14":1015363.0},"tbb::spin_mutex":{"1":20199929.0,"2":11734715.0,"3":7460630.0,"4":5116921.0,"5":4793972.0,"6":3313624.0,"7":2245725.0,"8":1473631.0,"9":943642.0,"10":757081.0,"11":575810.0,"12":492764.0,"13":486487.0,"14":424400.0}};
    const latency_9900KS = {"AtomicQueue":0.000000161,"AtomicQueue2":0.000000203,"AtomicQueueB":0.000000201,"AtomicQueueB2":0.000000206,"OptimistAtomicQueue":0.000000142,"OptimistAtomicQueue2":0.00000015,"OptimistAtomicQueueB":0.000000146,"OptimistAtomicQueueB2":0.000000196,"boost::lockfree::queue":0.000000311,"boost::lockfree::spsc_queue":0.000000127,"moodycamel::ConcurrentQueue":0.000000225,"moodycamel::ReaderWriterQueue":0.000000109,"pthread_spinlock":0.00000024,"std::mutex":0.00000043,"tbb::concurrent_bounded_queue":0.000000268,"tbb::spin_mutex":0.000000227,"xenium::michael_scott_queue":0.00000036,"xenium::ramalhete_queue":0.000000253,"xenium::vyukov_bounded_queue":0.000000185};
    const latency_xeon_gold_6132 = {"AtomicQueue":0.000000233,"AtomicQueue2":0.000000309,"AtomicQueueB":0.000000333,"AtomicQueueB2":0.000000387,"OptimistAtomicQueue":0.000000284,"OptimistAtomicQueue2":0.000000326,"OptimistAtomicQueueB":0.000000324,"OptimistAtomicQueueB2":0.00000035,"boost::lockfree::queue":0.000000695,"boost::lockfree::spsc_queue":0.000000256,"moodycamel::ConcurrentQueue":0.000000393,"moodycamel::ReaderWriterQueue":0.00000022,"pthread_spinlock":0.000000649,"tbb::concurrent_bounded_queue":0.000000593,"tbb::spin_mutex":0.000000515};

    plot_scalability('scalability-9900KS-5GHz', scalability_to_series(scalability_9900KS), "Intel i9-9900KS (core 5GHz / uncore 4.7GHz)", 60e6, 300e6);
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock) (to be updated...)", 15e6, 300e6);
    plot_latency('latency-9900KS-5GHz', latency_to_series(latency_9900KS), "Intel i9-9900KS (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock) (to be updated...)");
});
