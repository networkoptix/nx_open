const fs = require('fs');
const webpack = require('webpack');
const merge = require('webpack-merge');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const BundleAnalyzerPlugin = require('webpack-bundle-analyzer').BundleAnalyzerPlugin;
const common = require('./webpack.common.js');
const ExtractTextPlugin = require("extract-text-webpack-plugin");

module.exports = merge(common, {
    devtool  : 'cheap-module-eval-source-map',
    devServer: {
        contentBase       : './dist',
        hot               : true,
        host              : '0.0.0.0',
        port              : 9000,
        proxy             : [
            {
                context: [ '/api/', '/gateway/' ],
                target : 'http://cloud-test.hdw.mx:80'

            },
            {
                context     : '/static/',
                target      : "https://0.0.0.0:9000",
                pathRewrite : { "^/static": "" },
                changeOrigin: true,
                secure      : false
            }
        ],
        https             : {
            key : fs.readFileSync('ssl_keys/server.key').toString(),
            cert: fs.readFileSync('ssl_keys/server.crt').toString()
        },
        historyApiFallback: {
            index: '/'
        }
    },
    plugins  : [
        new webpack.HotModuleReplacementPlugin(),
        // new BundleAnalyzerPlugin({analyzerHost:'0.0.0.0', analyzerPort:9001})

        new CopyWebpackPlugin([
            {
                from: 'images',
                to  : 'static/images'
            }
        ])
    ],
    module   : {
        rules: [
            {
                test: /\.(scss|css)$/,
                use : ExtractTextPlugin.extract({
                    fallback: 'style-loader',
                    use     : [
                        { loader: 'css-loader', options: { url: false, sourceMap: true } },
                        { loader: 'postcss-loader', options: { url: false, sourceMap: 'inline' } },
                        { loader: 'sass-loader', options: { url: false, sourceMap: true } }
                    ]
                    // use: [
                    //     {
                    //         loader : "css-loader?sourceMap",
                    //         options: {
                    //             url      : false,
                    //             sourceMap: true
                    //         }
                    //     },
                    //     {
                    //         loader : "sass-loader?sourceMap",
                    //         options: {
                    //             url      : false,
                    //             sourceMap: true
                    //         }
                    //     } ]
                })
            }
        ]
    }
});
