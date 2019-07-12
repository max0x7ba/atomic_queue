"use strict";

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

$(function() {
    const settings = {
     "boost::lockfree::spsc_queue": ['#8E44AD',  0],
          "boost::lockfree::queue": ['#AA73C2',  1],
   "moodycamel::ReaderWriterQueue": ['#E74C3C',  2],
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
            yAxis: { title: { text: 'latency, nanoseconds/round-trip' }, max: 700 },
            tooltip: { valueSuffix: ' nanoseconds' },
            series: series
        });
    }

    // TODO: load these from files.
    const scalability_7700k = {"AtomicQueue":{"1":53972512.0,"2":23633546.0,"3":17516991.0,"4":14432094.0},"AtomicQueue2":{"1":50623983.0,"2":23709053.0,"3":17062546.0,"4":14430309.0},"AtomicQueueB":{"1":49232211.0,"2":17465450.0,"3":14354123.0,"4":11923342.0},"AtomicQueueB2":{"1":46300047.0,"2":13391291.0,"3":12703558.0,"4":11405668.0},"OptimistAtomicQueue":{"1":98084207.0,"2":87945253.0,"3":65733721.0,"4":45246718.0},"OptimistAtomicQueue2":{"1":57012256.0,"2":43347534.0,"3":45201448.0,"4":43618543.0},"OptimistAtomicQueueB":{"1":56530633.0,"2":32468388.0,"3":30529467.0,"4":32611940.0},"OptimistAtomicQueueB2":{"1":40950341.0,"2":18152266.0,"3":21189650.0,"4":22339227.0},"boost::lockfree::queue":{"1":31904998.0,"2":9879410.0,"3":8728706.0,"4":7779936.0},"boost::lockfree::spsc_queue":{"1":136850299.0,"2":null,"3":null,"4":null},"moodycamel::ConcurrentQueue":{"1":44424326.0,"2":16573570.0,"3":17580362.0,"4":21352214.0},"moodycamel::ReaderWriterQueue":{"1":590701265.0,"2":null,"3":null,"4":null},"pthread_spinlock":{"1":61772939.0,"2":22539147.0,"3":18579921.0,"4":17451000.0},"tbb::concurrent_bounded_queue":{"1":35298687.0,"2":16728751.0,"3":14536481.0,"4":12259487.0},"tbb::speculative_spin_mutex":{"1":66911703.0,"2":46479250.0,"3":36933864.0,"4":29590681.0},"tbb::spin_mutex":{"1":81393452.0,"2":61467363.0,"3":44930427.0,"4":33078793.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":8732552.0,"2":4897463.0,"3":3699540.0,"4":3116249.0,"5":2875898.0,"6":2570505.0,"7":2365050.0,"8":2066254.0,"9":1919221.0,"10":1602002.0,"11":1475780.0,"12":1412168.0,"13":1267854.0,"14":1031089.0},"AtomicQueue2":{"1":8795493.0,"2":5261325.0,"3":3532080.0,"4":3149199.0,"5":3073910.0,"6":2682654.0,"7":2349534.0,"8":2073710.0,"9":1662238.0,"10":1687612.0,"11":1614511.0,"12":1260203.0,"13":1209336.0,"14":1091961.0},"AtomicQueueB":{"1":6884459.0,"2":3970292.0,"3":3274960.0,"4":2875416.0,"5":2927298.0,"6":2729042.0,"7":2525793.0,"8":1851445.0,"9":1709836.0,"10":1372421.0,"11":1365531.0,"12":1219836.0,"13":1069822.0,"14":1006871.0},"AtomicQueueB2":{"1":7096692.0,"2":3566506.0,"3":3068597.0,"4":2907144.0,"5":2497990.0,"6":2536744.0,"7":2340960.0,"8":1684108.0,"9":1551371.0,"10":1468126.0,"11":1226373.0,"12":1197505.0,"13":1092055.0,"14":968502.0},"OptimistAtomicQueue":{"1":80691358.0,"2":15236887.0,"3":18865423.0,"4":21338181.0,"5":21059470.0,"6":21307949.0,"7":21276601.0,"8":17742818.0,"9":16211541.0,"10":15770303.0,"11":16408408.0,"12":16187474.0,"13":17274554.0,"14":17879563.0},"OptimistAtomicQueue2":{"1":25064261.0,"2":10936017.0,"3":14179524.0,"4":15901798.0,"5":17534935.0,"6":18736775.0,"7":18345042.0,"8":15769028.0,"9":14418331.0,"10":15392560.0,"11":15095680.0,"12":16013688.0,"13":17671444.0,"14":16357544.0},"OptimistAtomicQueueB":{"1":21268010.0,"2":10357304.0,"3":9728801.0,"4":9114168.0,"5":9288996.0,"6":9332975.0,"7":9612123.0,"8":6310076.0,"9":6684038.0,"10":6662074.0,"11":6879333.0,"12":6916696.0,"13":6501550.0,"14":6757318.0},"OptimistAtomicQueueB2":{"1":5559355.0,"2":5405731.0,"3":5439570.0,"4":5680678.0,"5":5381515.0,"6":5932977.0,"7":6266675.0,"8":4026212.0,"9":3822463.0,"10":4339386.0,"11":4113051.0,"12":4220448.0,"13":4298085.0,"14":4181954.0},"boost::lockfree::queue":{"1":3521690.0,"2":2562428.0,"3":2348623.0,"4":2238333.0,"5":2049650.0,"6":1950615.0,"7":1864150.0,"8":1458324.0,"9":1301970.0,"10":1299679.0,"11":1071609.0,"12":1052382.0,"13":954405.0,"14":987648.0},"boost::lockfree::spsc_queue":{"1":156886919.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":10956346.0,"2":6483408.0,"3":5874500.0,"4":5784180.0,"5":5616371.0,"6":5292931.0,"7":4795679.0,"8":3611693.0,"9":3165925.0,"10":2997458.0,"11":3244888.0,"12":3061964.0,"13":3056272.0,"14":3041314.0},"moodycamel::ReaderWriterQueue":{"1":344601355.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":10046660.0,"2":4865772.0,"3":3917035.0,"4":3187833.0,"5":2661811.0,"6":2412251.0,"7":2218144.0,"8":1347553.0,"9":1149743.0,"10":1303061.0,"11":1106312.0,"12":964206.0,"13":944251.0,"14":886010.0},"tbb::concurrent_bounded_queue":{"1":6508822.0,"2":5122635.0,"3":3996680.0,"4":3185002.0,"5":2878370.0,"6":2667553.0,"7":2320961.0,"8":1976717.0,"9":1800481.0,"10":1658594.0,"11":1407365.0,"12":1272839.0,"13":1261426.0,"14":1040637.0},"tbb::speculative_spin_mutex":{"1":23985515.0,"2":14807618.0,"3":8831191.0,"4":7101930.0,"5":5694446.0,"6":4904758.0,"7":4402582.0,"8":2770367.0,"9":1846840.0,"10":1716283.0,"11":1487662.0,"12":1421521.0,"13":1386256.0,"14":1258354.0},"tbb::spin_mutex":{"1":29034526.0,"2":16668651.0,"3":9542044.0,"4":8259730.0,"5":5902428.0,"6":4290460.0,"7":3223405.0,"8":1870864.0,"9":1305360.0,"10":976697.0,"11":886608.0,"12":734539.0,"13":770759.0,"14":634378.0}};
    const latency_7700k = {"sec/round-trip":{"AtomicQueue":0.000000136,"AtomicQueue2":0.000000174,"AtomicQueueB":0.000000164,"AtomicQueueB2":0.000000187,"OptimistAtomicQueue":0.000000085,"OptimistAtomicQueue2":0.00000015,"OptimistAtomicQueueB":0.000000121,"OptimistAtomicQueueB2":0.000000184,"boost::lockfree::queue":0.000000247,"boost::lockfree::spsc_queue":0.000000118,"moodycamel::ConcurrentQueue":0.000000214,"moodycamel::ReaderWriterQueue":0.000000118,"pthread_spinlock":0.000000242,"tbb::concurrent_bounded_queue":0.000000251,"tbb::speculative_spin_mutex":0.000000359,"tbb::spin_mutex":0.000000209}};
    const latency_xeon_gold_6132 = {"sec/round-trip":{"AtomicQueue":0.000000352,"AtomicQueue2":0.000000365,"AtomicQueueB":0.000000382,"AtomicQueueB2":0.000000428,"OptimistAtomicQueue":0.000000185,"OptimistAtomicQueue2":0.000000221,"OptimistAtomicQueueB":0.00000028,"OptimistAtomicQueueB2":0.000000409,"boost::lockfree::queue":0.000000691,"boost::lockfree::spsc_queue":0.000000245,"moodycamel::ConcurrentQueue":0.000000451,"moodycamel::ReaderWriterQueue":0.000000257,"pthread_spinlock":0.000000335,"tbb::concurrent_bounded_queue":0.000000588,"tbb::speculative_spin_mutex":0.000000609,"tbb::spin_mutex":0.000000294}};

    plot_scalability('scalability-7700k-5GHz', scalability_to_series(scalability_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-7700k-5GHz', latency_to_series(latency_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
