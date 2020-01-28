const path = require('path');
const webpack = require('webpack');
const merge = require('webpack-merge');
const UglifyJSPlugin = require('uglifyjs-webpack-plugin');
const common = require('./webpack.health_monitor.js');
const ExtractTextPlugin = require('extract-text-webpack-plugin');
const CleanWebpackPlugin = require('clean-webpack-plugin');
const CopyWebpackPlugin = require('copy-webpack-plugin');

module.exports = merge(common, {
    plugins: [
        new CleanWebpackPlugin([ 'dist' ]),
        new UglifyJSPlugin({}),
        new webpack.HashedModuleIdsPlugin(),
    
        new CopyWebpackPlugin([
            {
                // Copy en_US lang file to have correct values
                // before Boris update translations
                from: '../app/language_compiled.json',
                to: '../../translations/en_US/language_compiled.json'
            }
        ])
    ],
    output: {
        filename  : 'scripts/[name].[chunkhash].js',
        path      : path.resolve(__dirname, 'dist'),
        publicPath: '/'
    },
    module : {
        rules: [
            {
                test   : /\.scss$/,
                include: /src/,
                loaders: [ 'raw-loader', 'sass-loader' ]
            },
            {
                test   : /\.s?css$/,
                exclude: /src/,
                use    : ExtractTextPlugin.extract({
                    fallback: 'style-loader',
                    use     : [
                        {
                            loader : 'css-loader',
                            options: {
                                url      : false,
                                sourceMap: false
                            }
                        },
                        {
                            loader : 'postcss-loader',
                            options: {
                                url      : false,
                                sourceMap: false
                            }
                        },
                        {
                            loader : 'sass-loader',
                            options: {
                                url      : false,
                                sourceMap: false
                            }
                        }
                    ]
                })
            }
        ]
    }
});
