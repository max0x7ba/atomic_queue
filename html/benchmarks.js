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
            yAxis: {
                title: { text: 'throughput, msg/sec' },
                max: 200e6 // TODO: Extract SPSC into a separate chart.
            },
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
            yAxis: { title: { text: 'latency, nanoseconds/round-trip' }, max: 900 },
            tooltip: { valueSuffix: ' nanoseconds' },
            series: series
        });
    }

    // TODO: load these from files.
    const scalability_7700k = {"AtomicQueue":{"1":63730029.0,"2":13408150.0,"3":13624331.0,"4":11422669.0},"AtomicQueue2":{"1":79446170.0,"2":13028151.0,"3":13581704.0,"4":11269057.0},"AtomicQueueB":{"1":75345795.0,"2":12109991.0,"3":13082972.0,"4":11369317.0},"AtomicQueueB2":{"1":50856114.0,"2":11298254.0,"3":12204304.0,"4":11384890.0},"OptimistAtomicQueue":{"1":833944396.0,"2":38160227.0,"3":45197381.0,"4":50505275.0},"OptimistAtomicQueue2":{"1":519860914.0,"2":33751159.0,"3":44270032.0,"4":48492133.0},"OptimistAtomicQueueB":{"1":143785572.0,"2":23125417.0,"3":27019225.0,"4":30044468.0},"OptimistAtomicQueueB2":{"1":46514680.0,"2":18065646.0,"3":22838344.0,"4":28394547.0},"boost::lockfree::queue":{"1":9707992.0,"2":8209905.0,"3":8151069.0,"4":6393603.0},"boost::lockfree::spsc_queue":{"1":158115385.0,"2":null,"3":null,"4":null},"moodycamel::ConcurrentQueue":{"1":24745042.0,"2":13673705.0,"3":17299150.0,"4":20983338.0},"moodycamel::ReaderWriterQueue":{"1":440398741.0,"2":null,"3":null,"4":null},"pthread_spinlock":{"1":22769146.0,"2":19594281.0,"3":15042584.0,"4":12265430.0},"tbb::concurrent_bounded_queue":{"1":15747839.0,"2":15966477.0,"3":15214559.0,"4":13672051.0},"tbb::speculative_spin_mutex":{"1":46956112.0,"2":36723912.0,"3":31923896.0,"4":24737690.0},"tbb::spin_mutex":{"1":57149327.0,"2":37641701.0,"3":35641387.0,"4":31446122.0}};
    const scalability_xeon_gold_6132 = {"AtomicQueue":{"1":166633745.0,"2":6056106.0,"3":3740105.0,"4":3116638.0,"5":2626907.0,"6":2293317.0,"7":2076041.0,"8":2045313.0,"9":1877778.0,"10":1742052.0,"11":1594655.0,"12":1468472.0,"13":1332226.0,"14":1262979.0},"AtomicQueue2":{"1":126285403.0,"2":5310717.0,"3":4161137.0,"4":3167308.0,"5":2609934.0,"6":2285116.0,"7":2070151.0,"8":2028734.0,"9":1875103.0,"10":1723343.0,"11":1583281.0,"12":1467810.0,"13":1330867.0,"14":1254999.0},"AtomicQueueB":{"1":19645964.0,"2":4893636.0,"3":3556547.0,"4":2928685.0,"5":2563350.0,"6":2291118.0,"7":2051423.0,"8":1896251.0,"9":1789511.0,"10":1651728.0,"11":1525099.0,"12":1420154.0,"13":1289824.0,"14":1167661.0},"AtomicQueueB2":{"1":20989429.0,"2":4503745.0,"3":3375002.0,"4":2923434.0,"5":2617038.0,"6":2251080.0,"7":2038197.0,"8":1885399.0,"9":1793202.0,"10":1653396.0,"11":1532218.0,"12":1408672.0,"13":1287463.0,"14":1168865.0},"OptimistAtomicQueue":{"1":623455449.0,"2":13798329.0,"3":14649118.0,"4":15151313.0,"5":15078801.0,"6":15062360.0,"7":14965666.0,"8":12707992.0,"9":12671911.0,"10":12627555.0,"11":12484164.0,"12":12275698.0,"13":11815079.0,"14":11824101.0},"OptimistAtomicQueue2":{"1":310140324.0,"2":12426698.0,"3":13344912.0,"4":13913794.0,"5":14208265.0,"6":14206722.0,"7":14373602.0,"8":11588766.0,"9":11821545.0,"10":11914685.0,"11":11938272.0,"12":12279711.0,"13":12119191.0,"14":11451784.0},"OptimistAtomicQueueB":{"1":71321988.0,"2":8325801.0,"3":7961033.0,"4":7967029.0,"5":7874923.0,"6":7804313.0,"7":7802715.0,"8":6711805.0,"9":6598149.0,"10":6470750.0,"11":6410935.0,"12":6327852.0,"13":5964455.0,"14":6156487.0},"OptimistAtomicQueueB2":{"1":32756674.0,"2":7292917.0,"3":7172521.0,"4":7129716.0,"5":7193490.0,"6":7041266.0,"7":7111803.0,"8":6144936.0,"9":5924571.0,"10":5839749.0,"11":5829513.0,"12":5833375.0,"13":5631996.0,"14":5343359.0},"boost::lockfree::queue":{"1":3235071.0,"2":2709807.0,"3":2549486.0,"4":2462777.0,"5":2425524.0,"6":2257265.0,"7":2123762.0,"8":1562301.0,"9":1386942.0,"10":1247571.0,"11":1164023.0,"12":1096461.0,"13":1019487.0,"14":990571.0},"boost::lockfree::spsc_queue":{"1":72696575.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"moodycamel::ConcurrentQueue":{"1":9920083.0,"2":6907446.0,"3":6621133.0,"4":7195639.0,"5":7210350.0,"6":7414479.0,"7":7500038.0,"8":6032773.0,"9":5488520.0,"10":5371426.0,"11":5130999.0,"12":5080061.0,"13":5136583.0,"14":5167650.0},"moodycamel::ReaderWriterQueue":{"1":326002130.0,"2":null,"3":null,"4":null,"5":null,"6":null,"7":null,"8":null,"9":null,"10":null,"11":null,"12":null,"13":null,"14":null},"pthread_spinlock":{"1":10895062.0,"2":6427523.0,"3":4881778.0,"4":3572581.0,"5":3077369.0,"6":2657844.0,"7":2328652.0,"8":2030264.0,"9":1807268.0,"10":1638064.0,"11":1485292.0,"12":1342281.0,"13":1222955.0,"14":1177552.0},"tbb::concurrent_bounded_queue":{"1":7557523.0,"2":5768581.0,"3":4463424.0,"4":3801082.0,"5":3384164.0,"6":2952402.0,"7":2689135.0,"8":2288510.0,"9":2215077.0,"10":2032725.0,"11":1833543.0,"12":1642312.0,"13":1518829.0,"14":1468633.0},"tbb::speculative_spin_mutex":{"1":28527739.0,"2":13582767.0,"3":7709736.0,"4":5725200.0,"5":4658818.0,"6":3814202.0,"7":3106648.0,"8":2880793.0,"9":2303112.0,"10":1940944.0,"11":1757100.0,"12":1583888.0,"13":1443153.0,"14":1311289.0},"tbb::spin_mutex":{"1":30985014.0,"2":17576655.0,"3":9738749.0,"4":5252428.0,"5":3015681.0,"6":2046818.0,"7":1635670.0,"8":919715.0,"9":895536.0,"10":891474.0,"11":828907.0,"12":775847.0,"13":693887.0,"14":614419.0}};
    const latency_7700k = {"sec/round-trip":{"AtomicQueue":0.000000137,"AtomicQueue2":0.000000136,"AtomicQueueB":0.000000139,"AtomicQueueB2":0.00000017,"OptimistAtomicQueue":0.000000121,"OptimistAtomicQueue2":0.000000147,"OptimistAtomicQueueB":0.000000137,"OptimistAtomicQueueB2":0.000000167,"boost::lockfree::queue":0.000000265,"boost::lockfree::spsc_queue":0.000000125,"moodycamel::ConcurrentQueue":0.000000214,"moodycamel::ReaderWriterQueue":0.000000116,"pthread_spinlock":0.000004444,"tbb::concurrent_bounded_queue":0.000000249,"tbb::speculative_spin_mutex":0.000000766,"tbb::spin_mutex":0.000000201}};
    const latency_xeon_gold_6132 = {"sec/round-trip":{"AtomicQueue":0.000000252,"AtomicQueue2":0.000000316,"AtomicQueueB":0.00000033,"AtomicQueueB2":0.000000418,"OptimistAtomicQueue":0.000000292,"OptimistAtomicQueue2":0.000000325,"OptimistAtomicQueueB":0.000000367,"OptimistAtomicQueueB2":0.000000428,"boost::lockfree::queue":0.000000749,"boost::lockfree::spsc_queue":0.00000025,"moodycamel::ConcurrentQueue":0.000000462,"moodycamel::ReaderWriterQueue":0.000000246,"pthread_spinlock":0.000000306,"tbb::concurrent_bounded_queue":0.000000616,"tbb::speculative_spin_mutex":0.000000788,"tbb::spin_mutex":0.000000249}};

    plot_scalability('scalability-7700k-5GHz', scalability_to_series(scalability_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_scalability('scalability-xeon-gold-6132', scalability_to_series(scalability_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
    plot_latency('latency-7700k-5GHz', latency_to_series(latency_7700k), "Intel i7-7700k (core 5GHz / uncore 4.7GHz)");
    plot_latency('latency-xeon-gold-6132', latency_to_series(latency_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
