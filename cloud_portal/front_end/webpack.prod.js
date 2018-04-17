const path = require('path');
const webpack = require('webpack');
const merge = require('webpack-merge');
const UglifyJSPlugin = require('uglifyjs-webpack-plugin');
const common = require('./webpack.common.js');
const ExtractTextPlugin = require("extract-text-webpack-plugin");
const CleanWebpackPlugin = require('clean-webpack-plugin');

const ENV = process.env.ENV = process.env.NODE_ENV = 'prod';

module.exports = merge(common, {
    plugins: [
        new CleanWebpackPlugin([ 'dist' ]),
        new UglifyJSPlugin({}),
        new webpack.HashedModuleIdsPlugin(),
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
                    fallback: "style-loader",
                    use     : [
                        {
                            loader : 'css-loader',
                            options: {
                                url      : false,
                                // minimize : true,
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
