'use strict';

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.


const theme_axis = {
    gridLineColor: '#444',
    labels: { style: { color: '#eee' } },
    title: { style: { color: '#eee' } }
};

Highcharts.theme = {
    chart: {
        backgroundColor: 'black',
        style: { fontFamily: "'Roboto Slab', sans-serif" }
    },
    title: {
        text: undefined,
        style: {
            color: 'white',
            fontSize: '20px'
        }
    },
    subtitle: { style: { color: '#aaa' } },
    xAxis: theme_axis,
    yAxis: theme_axis,
    tooltip: {
        followPointer: true,
        shared: true,
        useHTML: true,
        backgroundColor: 'rgba(0, 0, 0, 0.85)',
        borderWidth: 1,
        borderColor: 'white',
        style: { color: 'white' }
    },
    plotOptions: {
        series: {
            pointPadding: 0.2,
            groupPadding: 0.1,
            borderWidth: 0,
            animationLimit: Infinity,
            shadow: true,
        },
        boxplot: {
            fillColor: 'rgba(0, 0, 0, 0)',
            grouping: false,
            lineWidth: 2,
            stemDashStyle: 'solid',
            stemWidth: 1,
            whiskerWidth: 3,
        },
        column: {
            borderWidth: 0
        },
        bar: {
            grouping: false,
            borderWidth: 0
        },
        errorbar: {
            stemWidth: 1,
            whiskerLength: 3,
            lineWidth: 1,
            dashStyle: 'Solid',
            enableMouseTracking: false,
            showInLegend: false,
            color: "#aaa",
        },
    },
    legend: {
        layout: 'horizontal',
        align: 'center',
        verticalAlign: 'bottom',
        itemStyle: { color: '#ccc' },
        itemHoverStyle: { color: 'white' },
        itemHiddenStyle: { color: '#606060' }
    },
    lang: { thousandsSep: ',' },
    credits: { enabled: false },
    accessibility: { enabled: false }
};

// Apply the theme
Highcharts.setOptions(Highcharts.theme);
