const merge = require('webpack-merge');
const UglifyJSPlugin = require('uglifyjs-webpack-plugin');
const common = require('./webpack.common.js');
const ExtractTextPlugin = require("extract-text-webpack-plugin");
const CleanWebpackPlugin = require('clean-webpack-plugin');

module.exports = merge(common, {
    plugins: [
        new CleanWebpackPlugin([ 'dist' ]),
        new UglifyJSPlugin({})
    ],
    module : {
        rules: [
            {
                test: /\.s?css$/,
                use : ExtractTextPlugin.extract({
                    fallback: "style-loader",
                    use     : [
                        {
                            loader : 'css-loader',
                            options: {
                                url      : false,
                                //minimize : true,
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
