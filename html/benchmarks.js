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
    const scalability_7700k = {"AtomicQueue":{"1":27480485.0,"2":14569157.0,"3":12931967.0},"AtomicQueue2":{"1":26452767.0,"2":14616236.0,"3":12823498.0},"BlockingAtomicQueue":{"1":119346550.0,"2":46267662.0,"3":53199578.0},"BlockingAtomicQueue2":{"1":75140383.0,"2":38691918.0,"3":38757784.0},"boost::lockfree::queue":{"1":9661869.0,"2":8213342.0,"3":8273477.0},"boost::lockfree::spsc_queue":{"1":251997545.0,"2":null,"3":null},"moodycamel::ConcurrentQueue":{"1":20245021.0,"2":12820195.0,"3":15885204.0},"pthread_spinlock":{"1":41738740.0,"2":18508600.0,"3":14830999.0},"tbb::concurrent_bounded_queue":{"1":15636497.0,"2":15905864.0,"3":14566885.0},"tbb::speculative_spin_mutex":{"1":55890715.0,"2":38532479.0,"3":31403577.0},"tbb::spin_mutex":{"1":84115672.0,"2":56798609.0,"3":39322375.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":8651668.0,"2":4968941.0,"3":3683433.0,"4":3132166.0,"5":2904792.0,"6":2501441.0,"7":1440061.0,"8":1088100.0,"9":1063403.0,"10":1108646.0,"11":1036841.0,"12":1078988.0,"13":1180694.0},"AtomicQueue2":{"1":10130602.0,"2":5003208.0,"3":3548190.0,"4":2855854.0,"5":2769759.0,"6":2673378.0,"7":1336336.0,"8":1127980.0,"9":1070180.0,"10":1072871.0,"11":1115495.0,"12":1140078.0,"13":1249730.0},"BlockingAtomicQueue":{"1":59693531.0,"2":11800036.0,"3":14568786.0,"4":15321197.0,"5":15140085.0,"6":19606221.0,"7":14671926.0,"8":15203509.0,"9":15671514.0,"10":16153284.0,"11":16560016.0,"12":17896080.0,"13":21504122.0},"BlockingAtomicQueue2":{"1":26168905.0,"2":12074730.0,"3":15164677.0,"4":16067618.0,"5":17905619.0,"6":14028380.0,"7":16145968.0,"8":14583260.0,"9":15337181.0,"10":15643690.0,"11":16158847.0,"12":17669598.0,"13":21016313.0},"boost::lockfree::queue":{"1":3266871.0,"2":2542323.0,"3":2345333.0,"4":2182187.0,"5":2003591.0,"6":1863143.0,"7":1382560.0,"8":871152.0,"9":877899.0,"10":822737.0,"11":832595.0,"12":702563.0,"13":708480.0},"boost::lockfree::spsc_queue":{"1":103053409.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null},"moodycamel::ConcurrentQueue":{"1":7914035.0,"2":5551079.0,"3":5409742.0,"4":5573217.0,"5":5887913.0,"6":6093806.0,"7":3982560.0,"8":4000741.0,"9":4134115.0,"10":4222205.0,"11":4229825.0,"12":4310331.0,"13":4543981.0},"pthread_spinlock":{"1":8638980.0,"2":4979835.0,"3":3722054.0,"4":2645192.0,"5":1835128.0,"6":2022431.0,"7":1119543.0,"8":890076.0,"9":842013.0,"10":585498.0,"11":730614.0,"12":628214.0,"13":1018729.0},"tbb::concurrent_bounded_queue":{"1":6135313.0,"2":5114928.0,"3":3527494.0,"4":3166730.0,"5":3165826.0,"6":2716783.0,"7":1410419.0,"8":1149550.0,"9":1066826.0,"10":1039772.0,"11":1176357.0,"12":1132745.0,"13":1222088.0},"tbb::speculative_spin_mutex":{"1":21898534.0,"2":12132171.0,"3":8235042.0,"4":6002924.0,"5":6002881.0,"6":4837971.0,"7":2772219.0,"8":1827043.0,"9":1515169.0,"10":1446513.0,"11":1383558.0,"12":1345158.0,"13":1318716.0},"tbb::spin_mutex":{"1":22641671.0,"2":11094817.0,"3":6865243.0,"4":5571108.0,"5":4299743.0,"6":3444430.0,"7":1739030.0,"8":951427.0,"9":654755.0,"10":605509.0,"11":574943.0,"12":556493.0,"13":464592.0}};
    const latency_7700k = {"sec\/round-trip":{"AtomicQueue":0.000000146,"AtomicQueue2":0.000000181,"BlockingAtomicQueue":0.000000088,"BlockingAtomicQueue2":0.000000151,"boost::lockfree::queue":0.000000257,"boost::lockfree::spsc_queue":0.000000094,"moodycamel::ConcurrentQueue":0.0000002,"pthread_spinlock":0.000000215,"tbb::concurrent_bounded_queue":0.000000249,"tbb::speculative_spin_mutex":0.00000041,"tbb::spin_mutex":0.000000178}}
    const latency_xeon_gold_6132 = {"sec\/round-trip":{"AtomicQueue":0.000000345,"AtomicQueue2":0.0000004,"BlockingAtomicQueue":0.000000204,"BlockingAtomicQueue2":0.000000319,"boost::lockfree::queue":0.000000642,"boost::lockfree::spsc_queue":0.00000028,"moodycamel::ConcurrentQueue":0.000000436,"pthread_spinlock":0.0000003,"tbb::concurrent_bounded_queue":0.000000594,"tbb::speculative_spin_mutex":0.000000588,"tbb::spin_mutex":0.000000257}}

    plot_scalability('scalability-7700k-5GHz', scalability_to_series(scalability_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-7700k-5GHz', latency_to_series(latency_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
