'use strict';

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

Highcharts.theme = {
    chart: {
        backgroundColor: 'black',
        style: { fontFamily: "'Roboto Slab', sans-serif" }
    },
    title: {
        style: {
            color: 'white',
            fontSize: '20px'
        }
    },
    subtitle: {
        style: {
            color: '#808080'
        }
    },
    xAxis: {
        gridLineColor: '#808080',
        labels: {
            style: {
                color: '#E0E0E0'
            }
        },
        lineColor: '#707070',
        minorGridLineColor: '#505050',
        tickColor: '#707070',
        title: {
            style: {
                color: 'white'
            }
        }
    },
    yAxis: {
        gridLineColor: '#808080',
        labels: {
            style: {
                color: '#E0E0E0'
            }
        },
        lineColor: '#707070',
        minorGridLineColor: '#505050',
        tickColor: '#707070',
        tickWidth: 1,
        title: {
            style: {
                color: 'white'
            }
        }
    },
    tooltip: {
        backgroundColor: 'rgba(0, 0, 0, 0.85)',
        borderWidth: 1,
        borderColor: 'white',
        style: {
            color: '#F0F0F0'
        }
    },
    plotOptions: {
        series: {
            pointPadding: 0.2,
            groupPadding: 0.1,
            borderWidth: 0,
            animationLimit: Infinity,
            shadow: true,
            dataLabels: {
                color: '#B0B0B0'
            },
            marker: {
                lineColor: '#333'
            }
        },
        boxplot: {
            fillColor: '#505050'
        },
        column: {
            borderWidth: 0
        },
        bar: {
            borderWidth: 0
        },
        candlestick: {
            lineColor: 'white'
        },
        errorbar: {
            color: 'white'
        }
    },
    legend: {
        layout: 'horizontal',
        align: 'center',
        verticalAlign: 'bottom',
        itemStyle: {
            color: '#808080'
        },
        itemHoverStyle: {
            color: '#FFF'
        },
        itemHiddenStyle: {
            color: '#606060'
        }
    },
    credits: {
        style: {
            color: '#666'
        }
    },
    labels: {
        style: {
            color: '#707070'
        }
    },
    drilldown: {
        activeAxisLabelStyle: {
            color: '#F0F0F0'
        },
        activeDataLabelStyle: {
            color: '#F0F0F0'
        }
    },
    navigation: {
        buttonOptions: {
            symbolStroke: '#DDDDDD',
            theme: {
                fill: '#505050'
            }
        }
    },
    // scroll charts
    rangeSelector: {
        buttonTheme: {
            fill: '#505050',
            stroke: '#000000',
            style: {
                color: '#CCC'
            },
            states: {
                hover: {
                    fill: '#707070',
                    stroke: '#000000',
                    style: {
                        color: 'white'
                    }
                },
                select: {
                    fill: '#000003',
                    stroke: '#000000',
                    style: {
                        color: 'white'
                    }
                }
            }
        },
        inputBoxBorderColor: '#505050',
        inputStyle: {
            backgroundColor: '#333',
            color: 'silver'
        },
        labelStyle: {
            color: 'silver'
        }
    },
    navigator: {
        handles: {
            backgroundColor: '#666',
            borderColor: '#AAA'
        },
        outlineColor: '#CCC',
        maskFill: 'rgba(255,255,255,0.1)',
        series: {
            color: '#7798BF',
            lineColor: '#A6C7ED'
        },
        xAxis: {
            gridLineColor: '#505050'
        }
    },
    scrollbar: {
        barBackgroundColor: '#808080',
        barBorderColor: '#808080',
        buttonArrowColor: '#CCC',
        buttonBackgroundColor: '#606060',
        buttonBorderColor: '#606060',
        rifleColor: '#FFF',
        trackBackgroundColor: '#404043',
        trackBorderColor: '#404043'
    },
    // special colors for some of the
    legendBackgroundColor: 'rgba(0, 0, 0, 0.5)',
    background2: '#505050',
    dataLabelsColor: '#B0B0B0',
    textColor: '#C0C0C0',
    contrastTextColor: '#F0F0F0',
    maskColor: 'rgba(255,255,255,0.3)',

    lang: { thousandsSep: ',' },
    credits: { enabled: false },
    accessibility: { enabled: false }
};

// Apply the theme
Highcharts.setOptions(Highcharts.theme);
