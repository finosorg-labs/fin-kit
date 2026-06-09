module github.com/finosorg-labs/fin-kit/broker

go 1.26.1

require (
	github.com/finosorg-labs/exchange v1.0.1
	github.com/finosorg-labs/exchange-c/orderbook v1.0.6
)

require (
	github.com/finosorg-labs/ds-c/sdk v1.1.4 // indirect
	github.com/finosorg-labs/hash-c/sdk v1.0.2 // indirect
)

replace github.com/finosorg-labs/exchange => ../core/exchange
