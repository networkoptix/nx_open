const fs = require('fs');
const path = require('path');
const webpack = require('webpack');
const merge = require('webpack-merge');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const BundleAnalyzerPlugin = require('webpack-bundle-analyzer').BundleAnalyzerPlugin;
const common = require('./webpack.common.js');
const ExtractTextPlugin = require("extract-text-webpack-plugin");

const ENV = process.env.ENV = process.env.NODE_ENV = 'dev';

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

        // make some resources available while serve the project locally
        new CopyWebpackPlugin([
            {
                from: 'images',
                to  : 'static/images'
            },
            // {
            //     from: '../../translations/en_US',
            //     to  : 'static/lang_en_US'
            // },
            // {
            //     from: '../../translations/es_ES',
            //     to  : 'static/lang_es_ES'
            // },
            {
                from: '../../translations/ru_RU/',
                to  : 'static/lang_ru_RU/'
            }
            // ,
            // {
            //     from: '../../translations/ru_RU/views/dialogs/login.html',
            //     to  : 'translations/lang_ru_RU/views/dialogs/login.html'
            // }
        ])
    ],
    output: {
        filename  : 'scripts/[name].js',
        // path      : path.resolve(__dirname, 'dist'),
        publicPath: '/'
    },
    module   : {
        rules: [
            {
                test   : /\.scss$/,
                include: /src/,
                loaders: [ 'raw-loader', 'sass-loader' ]
            },
            {
                test: /\.s?css$/,
                exclude: /src/,
                use : ExtractTextPlugin.extract({
                    fallback: "style-loader",
                    use     : [
                        {
                            loader : 'css-loader',
                            options: {
                                url      : false,
                                //minimize : true,
                                sourceMap: true
                            }
                        },
                        {
                            loader : 'postcss-loader',
                            options: {
                                url      : false,
                                sourceMap: 'inline'
                            }
                        },
                        {
                            loader : 'sass-loader',
                            options: {
                                url      : false,
                                sourceMap: true
                            }
                        }
                    ]
                })
            }
        ]
    }
});
