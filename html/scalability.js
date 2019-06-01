"use strict";

$(function() {
    const settings = {
     "boost::lockfree::spsc_queue": ['#9B59B6', 0],
          "boost::lockfree::queue": ['#76448A', 1],
                "pthread_spinlock": ['#58D68D', 2],
     "moodycamel::ConcurrentQueue": ['#BA4A00', 3],
                 "tbb::spin_mutex": ['#5DADE2', 4],
     "tbb::speculative_spin_mutex": ['#2E86C1', 5],
   "tbb::concurrent_bounded_queue": ['#21618C', 6],
                     "AtomicQueue": ['#F1C40F', 7],
                    "AtomicQueue2": ['#F39C12', 8],
             "BlockingAtomicQueue": ['#C0392B', 9],
            "BlockingAtomicQueue2": ['#E74C3C', 10],
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
                data: [{y: value * 1e9, x: s[1]}]
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
            plotOptions: { series: { stacking: 'normal' } },
            title: { text: 'Latency on ' + title_suffix },
            xAxis: { categories: categories },
            yAxis: { title: { text: 'letency, nanoseconds/round-trip' } },
            series: series
        });
    }

    // TODO: load these from files.
    const scalability_7700k = {"AtomicQueue":{"1":27042489.0,"2":14433267.0,"3":13174470.0},"AtomicQueue2":{"1":26956305.0,"2":14416246.0,"3":13116088.0},"BlockingAtomicQueue":{"1":118101900.0,"2":48069873.0,"3":45423479.0},"BlockingAtomicQueue2":{"1":99226630.0,"2":39737559.0,"3":54349231.0},"boost::lockfree::queue":{"1":11023483.0,"2":8099673.0,"3":8206546.0},"boost::lockfree::spsc_queue":{"1":200912400.0,"2":null,"3":null},"moodycamel::ConcurrentQueue":{"1":21877814.0,"2":13313696.0,"3":16899773.0},"pthread_spinlock":{"1":41645830.0,"2":15673894.0,"3":14472407.0},"tbb::concurrent_bounded_queue":{"1":15714751.0,"2":16053555.0,"3":14006100.0},"tbb::speculative_spin_mutex":{"1":54891133.0,"2":38183081.0,"3":31167841.0},"tbb::spin_mutex":{"1":84869062.0,"2":54668038.0,"3":37958525.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":10876679.0,"2":5180138.0,"3":3819595.0,"4":3217122.0,"5":3017459.0,"6":2722561.0,"7":1490056.0,"8":1278662.0,"9":1270394.0,"10":1255603.0,"11":1355484.0,"12":1341954.0,"13":1350106.0},"AtomicQueue2":{"1":10759484.0,"2":5277380.0,"3":3557626.0,"4":3008402.0,"5":2733786.0,"6":2603402.0,"7":1420849.0,"8":1280821.0,"9":1274808.0,"10":1312496.0,"11":1392093.0,"12":1336302.0,"13":1319230.0},"BlockingAtomicQueue":{"1":86410344.0,"2":17162337.0,"3":19023817.0,"4":20938222.0,"5":21692927.0,"6":20439739.0,"7":17031623.0,"8":16755863.0,"9":16788438.0,"10":16413155.0,"11":16912252.0,"12":18181788.0,"13":19839626.0},"BlockingAtomicQueue2":{"1":26950145.0,"2":12190558.0,"3":13485867.0,"4":17495970.0,"5":16804263.0,"6":14726468.0,"7":14647858.0,"8":15170010.0,"9":15439516.0,"10":15293454.0,"11":16381086.0,"12":17706679.0,"13":16665156.0},"boost::lockfree::queue":{"1":3076177.0,"2":2585288.0,"3":2308909.0,"4":2077551.0,"5":2132701.0,"6":1910673.0,"7":1272620.0,"8":995984.0,"9":971658.0,"10":935901.0,"11":913673.0,"12":981122.0,"13":952212.0},"boost::lockfree::spsc_queue":{"1":28647057.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null},"moodycamel::ConcurrentQueue":{"1":9114627.0,"2":7268810.0,"3":6919174.0,"4":6846519.0,"5":7140036.0,"6":6757518.0,"7":5071657.0,"8":4363453.0,"9":4343497.0,"10":4453191.0,"11":4527464.0,"12":4537660.0,"13":4666488.0},"pthread_spinlock":{"1":11181241.0,"2":4712279.0,"3":2948019.0,"4":2700026.0,"5":2152777.0,"6":2024179.0,"7":928253.0,"8":1171516.0,"9":1004067.0,"10":933977.0,"11":1092945.0,"12":986337.0,"13":1014406.0},"tbb::concurrent_bounded_queue":{"1":6765902.0,"2":5241217.0,"3":4004545.0,"4":3515826.0,"5":3003594.0,"6":2819324.0,"7":1932210.0,"8":1232510.0,"9":1195961.0,"10":1249509.0,"11":1310556.0,"12":1326741.0,"13":1346596.0},"tbb::speculative_spin_mutex":{"1":25288998.0,"2":13186085.0,"3":8743569.0,"4":7999963.0,"5":3985715.0,"6":4093998.0,"7":2441846.0,"8":1637582.0,"9":1410427.0,"10":1347447.0,"11":1204232.0,"12":1156347.0,"13":1172530.0},"tbb::spin_mutex":{"1":29135222.0,"2":15306010.0,"3":10198837.0,"4":5864980.0,"5":4116864.0,"6":2988234.0,"7":1730819.0,"8":972850.0,"9":629137.0,"10":540488.0,"11":468472.0,"12":421827.0,"13":366985.0}};
    const latency_7700k = {"sec\/round-trip":{"AtomicQueue":0.000000142,"AtomicQueue2":0.00000017,"BlockingAtomicQueue":0.000000093,"BlockingAtomicQueue2":0.000000184,"boost::lockfree::queue":0.000000269,"boost::lockfree::spsc_queue":0.000000104,"moodycamel::ConcurrentQueue":0.000000191,"pthread_spinlock":0.000000397,"tbb::concurrent_bounded_queue":0.000000248,"tbb::speculative_spin_mutex":0.000000502,"tbb::spin_mutex":0.000000623}};
    const latency_xeon_gold_6132 = {"sec\/round-trip":{"AtomicQueue":0.000000342,"AtomicQueue2":0.000000429,"BlockingAtomicQueue":0.000000211,"BlockingAtomicQueue2":0.000000377,"boost::lockfree::queue":0.000000661,"boost::lockfree::spsc_queue":0.000000311,"moodycamel::ConcurrentQueue":0.000000451,"pthread_spinlock":0.000001114,"tbb::concurrent_bounded_queue":0.000000611,"tbb::speculative_spin_mutex":0.000002353,"tbb::spin_mutex":0.000001995}};

    plot_scalability('scalability-7700k-5GHz', scalability_to_series(scalability_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-7700k-5GHz', latency_to_series(latency_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
