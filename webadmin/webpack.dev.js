const fs = require('fs');
const webpack = require('webpack');
const merge = require('webpack-merge');
const common = require('./webpack.common.js');

module.exports = merge(common, {
    devtool: 'inline-source-map',
    devServer:{
        contentBase: './dist',
        hot: true,
        host: 'localhost',
        port: 9000,
        proxy: [
            {
                context: ['/web/', '/api/', '/ec2/', '/hls/', '/media/', '/proxy/'],
                target: 'http://10.1.5.150:7001'
            },
        ],
        historyApiFallback: {
            index: '/',
        },
    }
    ,
    plugins:[
        new webpack.HotModuleReplacementPlugin()
    ]
});