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
   "moodycamel::ReaderWriterQueue": [$.extend(true, {pattern: {color: '#BA4A00'}}, spsc_pattern),  1],
                "pthread_spinlock": ['#58D68D',  2],
                      "std::mutex": ['#239B56',  3],
                 "tbb::spin_mutex": ['#3498DB',  4],
   "tbb::concurrent_bounded_queue": ['#9ACCED',  5],
          "boost::lockfree::queue": ['#AA73C2',  6],
     "moodycamel::ConcurrentQueue": ['#BA4A00',  7],
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
        series.sort((a, b) => { return a.index - b.index; });
        series.forEach((element, index) => {
            element.index = index;
            element.data[0].x = index;
        });
        const categories = series.map(s => { return s.name; });
        return [series, categories];
    }

    function plot_scalability(div_id, series, title_suffix, max_lin, max_log) {
        const modes = [
            {type: 'linear', title: { text: 'throughput, msg/sec (linear scale)'}, max: max_lin, min: 0 },
            {type: 'logarithmic', title: { text: 'throughput, msg/sec (logarithmic scale)'}, max: max_log, min: 100e3},
        ];
        let mode = 0;
        const chart = Highcharts.chart(div_id, {
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
            series: series
        });
    }

    function plot_latency(div_id, series_categories, title_suffix) {
        const [series, categories] = series_categories;
        Highcharts.chart(div_id, {
            chart: { type: 'bar' },
            plotOptions: {
                series: { stacking: 'normal'},
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
    const scalability_9900KS = {"AtomicQueue":{"1":286258811.0,"2":12511844.0,"3":10870735.0,"4":8262334.0,"5":8164594.0,"6":7993441.0,"7":8020563.0,"8":7515622.0},"AtomicQueue2":{"1":61696929.0,"2":12267302.0,"3":10001213.0,"4":8157226.0,"5":8097440.0,"6":7952026.0,"7":7995642.0,"8":7441272.0},"AtomicQueueB":{"1":228034973.0,"2":12122517.0,"3":11117123.0,"4":8149391.0,"5":7922868.0,"6":7623977.0,"7":7812016.0,"8":7214447.0},"AtomicQueueB2":{"1":44550020.0,"2":11624801.0,"3":9898647.0,"4":8026821.0,"5":7806120.0,"6":7518179.0,"7":7775829.0,"8":7175857.0},"OptimistAtomicQueue":{"1":829842979.0,"2":32761745.0,"3":37548890.0,"4":39453579.0,"5":48677211.0,"6":50559092.0,"7":53156863.0,"8":50602252.0},"OptimistAtomicQueue2":{"1":682547965.0,"2":29516399.0,"3":33834235.0,"4":36241653.0,"5":43357072.0,"6":49741386.0,"7":51839802.0,"8":49877392.0},"OptimistAtomicQueueB":{"1":738447042.0,"2":32474220.0,"3":37037279.0,"4":38905937.0,"5":47608460.0,"6":49368578.0,"7":51655899.0,"8":49215349.0},"OptimistAtomicQueueB2":{"1":495211858.0,"2":29825513.0,"3":34057706.0,"4":33672715.0,"5":40347303.0,"6":42786274.0,"7":45751323.0,"8":46736438.0},"boost::lockfree::queue":{"1":8277185.0,"2":7803259.0,"3":7648842.0,"4":7098293.0,"5":6773852.0,"6":6298061.0,"7":6154693.0,"8":5801836.0},"boost::lockfree::spsc_queue":{"1":78317500.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null},"moodycamel::ConcurrentQueue":{"1":29453011.0,"2":52431952.0,"3":43648056.0,"4":45837232.0,"5":47841058.0,"6":63384866.0,"7":66347129.0,"8":64375322.0},"moodycamel::ReaderWriterQueue":{"1":538733018.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null},"pthread_spinlock":{"1":29932694.0,"2":18312194.0,"3":12289865.0,"4":9383867.0,"5":10466994.0,"6":9328919.0,"7":8492547.0,"8":7596251.0},"std::mutex":{"1":9199394.0,"2":6598427.0,"3":6900299.0,"4":7100935.0,"5":6739344.0,"6":6571707.0,"7":6378700.0,"8":6181434.0},"tbb::concurrent_bounded_queue":{"1":14807665.0,"2":15166768.0,"3":12535211.0,"4":10656023.0,"5":10217187.0,"6":9516659.0,"7":8693321.0,"8":8013655.0},"tbb::spin_mutex":{"1":41937261.0,"2":21806602.0,"3":11563566.0,"4":6495310.0,"5":5929302.0,"6":5441256.0,"7":4907625.0,"8":4286720.0},"xenium::michael_scott_queue":{"1":10161353.0,"2":8706024.0,"3":7702336.0,"4":6500382.0,"5":6844074.0,"6":7118668.0,"7":6728490.0,"8":6324097.0},"xenium::ramalhete_queue":{"1":33285933.0,"2":24719839.0,"3":29464259.0,"4":34047588.0,"5":38717573.0,"6":40481895.0,"7":42470849.0,"8":44944634.0},"xenium::vyukov_bounded_queue":{"1":122827707.0,"2":29204433.0,"3":16468857.0,"4":12747022.0,"5":11532118.0,"6":12829017.0,"7":10421370.0,"8":8591244.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":158109112.0,"2":4921854.0,"3":3498735.0,"4":2896774.0,"5":2416926.0,"6":2046932.0,"7":1773634.0,"8":1645924.0,"9":1457036.0,"10":1322161.0,"11":1186336.0,"12":1072455.0,"13":930567.0,"14":931606.0},"AtomicQueue2":{"1":130966968.0,"2":4620760.0,"3":3305710.0,"4":2787070.0,"5":2364350.0,"6":1972774.0,"7":1816863.0,"8":1715741.0,"9":1543989.0,"10":1362488.0,"11":1200436.0,"12":1066522.0,"13":956885.0,"14":883559.0},"AtomicQueueB":{"1":150200425.0,"2":4731025.0,"3":3368096.0,"4":2829384.0,"5":2408528.0,"6":1979764.0,"7":1855659.0,"8":1707383.0,"9":1467147.0,"10":1362266.0,"11":1257940.0,"12":1118451.0,"13":986849.0,"14":911597.0},"AtomicQueueB2":{"1":30885730.0,"2":4940112.0,"3":3295637.0,"4":2695437.0,"5":2257248.0,"6":2044260.0,"7":1831373.0,"8":1714119.0,"9":1446334.0,"10":1345247.0,"11":1146609.0,"12":1102961.0,"13":951675.0,"14":946796.0},"OptimistAtomicQueue":{"1":615462112.0,"2":12588449.0,"3":13517952.0,"4":14099926.0,"5":14555742.0,"6":14477634.0,"7":14589043.0,"8":11942734.0,"9":12318122.0,"10":11652615.0,"11":11276576.0,"12":11790362.0,"13":11616924.0,"14":11580480.0},"OptimistAtomicQueue2":{"1":285701790.0,"2":11464345.0,"3":12643790.0,"4":13373738.0,"5":13587917.0,"6":13787959.0,"7":14214689.0,"8":11068029.0,"9":11508394.0,"10":10943725.0,"11":10735351.0,"12":10831674.0,"13":10856099.0,"14":11070676.0},"OptimistAtomicQueueB":{"1":392396088.0,"2":12772847.0,"3":13333742.0,"4":13799277.0,"5":14338043.0,"6":14249719.0,"7":14319209.0,"8":12205595.0,"9":11696373.0,"10":11075294.0,"11":11768276.0,"12":11481230.0,"13":11334782.0,"14":11157997.0},"OptimistAtomicQueueB2":{"1":52277970.0,"2":11010593.0,"3":11902777.0,"4":12363497.0,"5":12904686.0,"6":13074313.0,"7":13206227.0,"8":10537499.0,"9":10484867.0,"10":10087570.0,"11":10107976.0,"12":9929433.0,"13":10750117.0,"14":10061327.0},"boost::lockfree::queue":{"1":3509287.0,"2":2691360.0,"3":2524041.0,"4":2279338.0,"5":2090858.0,"6":1923587.0,"7":1794532.0,"8":1295226.0,"9":1214404.0,"10":1030892.0,"11":948879.0,"12":894742.0,"13":768881.0,"14":782735.0},"boost::lockfree::spsc_queue":{"1":192419130.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":11324231.0,"2":6256475.0,"3":6277392.0,"4":6300071.0,"5":5622547.0,"6":5854465.0,"7":5134036.0,"8":3802947.0,"9":3549189.0,"10":3286559.0,"11":3416412.0,"12":3376207.0,"13":3319388.0,"14":3502120.0},"moodycamel::ReaderWriterQueue":{"1":275435749.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":9636407.0,"2":4638371.0,"3":3549542.0,"4":2780490.0,"5":2484911.0,"6":2042073.0,"7":1893618.0,"8":1317140.0,"9":1074015.0,"10":934007.0,"11":912801.0,"12":852631.0,"13":827944.0,"14":823481.0},"tbb::concurrent_bounded_queue":{"1":6767479.0,"2":5453622.0,"3":4145085.0,"4":3564610.0,"5":3010331.0,"6":2587858.0,"7":2440643.0,"8":2068666.0,"9":2058159.0,"10":1739814.0,"11":1378381.0,"12":1234436.0,"13":1122814.0,"14":1015363.0},"tbb::spin_mutex":{"1":20199929.0,"2":11734715.0,"3":7460630.0,"4":5116921.0,"5":4793972.0,"6":3313624.0,"7":2245725.0,"8":1473631.0,"9":943642.0,"10":757081.0,"11":575810.0,"12":492764.0,"13":486487.0,"14":424400.0},"std::mutex":{"1":6310569.0,"2":4409320.0,"3":3131619.0,"4":3732458.0,"5":3944050.0,"6":4865090.0,"7":4817517.0,"8":3029023.0,"9":2494417.0,"10":2248690.0,"11":2079053.0,"12":2021206.0,"13":2019624.0,"14":2012028.0},"xenium::michael_scott_queue":{"1":4348585.0,"2":2801508.0,"3":2362747.0,"4":1948754.0,"5":1688837.0,"6":1646305.0,"7":1489044.0,"8":1269747.0,"9":1051668.0,"10":901028.0,"11":875082.0,"12":716284.0,"13":652457.0,"14":656109.0},"xenium::ramalhete_queue":{"1":15907512.0,"2":9859400.0,"3":11436032.0,"4":11298981.0,"5":11587121.0,"6":11508362.0,"7":11875851.0,"8":8625635.0,"9":8212694.0,"10":8154679.0,"11":8243399.0,"12":8866873.0,"13":8545991.0,"14":9573085.0},"xenium::vyukov_bounded_queue":{"1":21778808.0,"2":7204578.0,"3":4971188.0,"4":4092418.0,"5":3173930.0,"6":3066766.0,"7":2732360.0,"8":2133157.0,"9":2032751.0,"10":1777709.0,"11":1529543.0,"12":1323655.0,"13":1143511.0,"14":1172102.0}};
    const latency_9900KS = {"AtomicQueue":0.000000157,"AtomicQueue2":0.000000173,"AtomicQueueB":0.000000171,"AtomicQueueB2":0.000000175,"OptimistAtomicQueue":0.000000148,"OptimistAtomicQueue2":0.000000167,"OptimistAtomicQueueB":0.00000014,"OptimistAtomicQueueB2":0.000000149,"boost::lockfree::queue":0.00000031,"boost::lockfree::spsc_queue":0.000000129,"moodycamel::ConcurrentQueue":0.000000208,"moodycamel::ReaderWriterQueue":0.00000011,"pthread_spinlock":0.000000226,"std::mutex":0.000000411,"tbb::concurrent_bounded_queue":0.000000268,"tbb::spin_mutex":0.000000246,"xenium::michael_scott_queue":0.000000357,"xenium::ramalhete_queue":0.000000255,"xenium::vyukov_bounded_queue":0.000000183};
    const latency_xeon_gold_6132 = {"AtomicQueue":0.000000233,"AtomicQueue2":0.000000309,"AtomicQueueB":0.000000333,"AtomicQueueB2":0.000000387,"OptimistAtomicQueue":0.000000284,"OptimistAtomicQueue2":0.000000326,"OptimistAtomicQueueB":0.000000324,"OptimistAtomicQueueB2":0.00000035,"boost::lockfree::queue":0.000000695,"boost::lockfree::spsc_queue":0.000000256,"moodycamel::ConcurrentQueue":0.000000393,"moodycamel::ReaderWriterQueue":0.00000022,"pthread_spinlock":0.000000649,"tbb::concurrent_bounded_queue":0.000000593,"tbb::spin_mutex":0.000000515,"std::mutex":0.000001867,"xenium::michael_scott_queue":0.000000752,"xenium::ramalhete_queue":0.000000501,"xenium::vyukov_bounded_queue":0.000000427};

    plot_scalability('scalability-9900KS-5GHz', scalability_to_series(scalability_9900KS), "Intel i9-9900KS (core 5GHz / uncore 4.7GHz)", 65e6, 1000e6);
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)", 15e6, 300e6);
    plot_latency('latency-9900KS-5GHz', latency_to_series(latency_9900KS), "Intel i9-9900KS (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
