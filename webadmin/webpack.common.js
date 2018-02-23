const path = require('path');
const webpack = require('webpack');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const CleanWebpackPlugin = require('clean-webpack-plugin');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const ExtractTextPlugin = require("extract-text-webpack-plugin");

module.exports = {
  	context: path.resolve(__dirname + '/app'),
  	entry:{
        app: 'scripts/entrypoint-app.js',
		inline: 'scripts/entrypoint-inline.js',
		login: 'scripts/entrypoint-login.js',
		webcommonApp: 'web_common/scripts/entrypoint-app.js',
		webcommonInline: 'web_common/scripts/entrypoint-inline.js',
		webcommonLogin: 'web_common/scripts/entrypoint-login.js'
	},
	plugins: [
        //Development plugins
		new CleanWebpackPlugin(['dist']),

        //Plugins used for making templates
        new webpack.NamedModulesPlugin(),
		new HtmlWebpackPlugin({
			chunks: ['commons', 'app', 'webcommonApp'],
			template: 'index-template.html',
			filename: 'index.html'
		}),
		new HtmlWebpackPlugin({
			chunks: ['commons', 'inline', 'webcommonInline'],
			template: 'inline-template.html',
			filename: 'inline.html'
		}),
		new HtmlWebpackPlugin({
			chunks: ['commons', 'login', 'webcommonLogin'],
			template: 'login-template.html',
			filename: 'login.html'
		}),
        new ExtractTextPlugin({filename: 'styles/[name].css'}),
        new CopyWebpackPlugin([
			{
				from: '',
				to: '',
				ignore: ['web_common/**', 'styles/**', '.*', '*.js', '*-template.html']
			},
        	{
        		from: 'web_common',
        		to: 'web_common',
        		ignore: ['scripts/**', 'styles/**', 'chromeless/**', '*.js']
        	},
        ]),

        //Plugins for npm packages
        new webpack.ProvidePlugin({
			'md5': 'md5',
      		'$': 'jquery',
            'jQuery': 'jquery',
			'_': 'underscore',
			'Base64': 'base-64',
            'screenfull': 'screenfull'
    	}),
        new webpack.optimize.CommonsChunkPlugin({
          	name: "commons",
          	filename: "scripts/commons.js",
			minChunks: 2
      	})
	],
	output: {
		filename: 'scripts/[name].js',
		path: path.resolve(__dirname, 'dist'),
		publicPath: '/'
	},
    resolve:{
        alias:{
        	fonts: path.join(__dirname, 'app', 'fonts'),
            scripts: path.join(__dirname, 'app', 'scripts'),
            web_common: path.join(__dirname, 'app', 'web_common')
        }
    },
	module: {
		loaders: [
        	{
        		test: /\.js$/,
        		exclude: /node_modules/,
        		loader: 'babel-loader'
        	}
      	],
		rules: [
			{
            	test: /\.(scss|css)$/,
            	use: ExtractTextPlugin.extract({
                    use: [{
                        loader: "css-loader",
                        options:{
                            url: false
                        }
                    },
                    {
                        loader: "sass-loader",
                        options:{
                            url: false
                        }
                    }]
            	})
        	},
		]
	}
};